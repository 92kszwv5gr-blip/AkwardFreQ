#include <juce_core/juce_core.h>
#include "AllocationCounter.h"
#include "TestUtils.h"
#include "mastering/MasteringChain.h"

namespace afq
{
    // The audio callback must not allocate (CLAUDE.md). These run the real-time entry points with a counting
    // operator new / malloc and assert nothing was allocated.
    struct RealtimeSafetyTests : juce::UnitTest
    {
        RealtimeSafetyTests() : juce::UnitTest ("RealtimeSafety", "AkwardFreQ") {}

        void runTest() override
        {
            using namespace test;

            beginTest ("the counter itself works");
            {
                startCountingAllocations();
                auto* p = new int[16];
                const size_t counted = stopCountingAllocations();
                delete[] p;
                expectGreaterThan ((int) counted, 0, "an operator new is counted");
                if (countsMalloc())
                {
                    startCountingAllocations();
                    void* m = std::malloc (64);
                    const size_t countedMalloc = stopCountingAllocations();
                    std::free (m);
                    expectGreaterThan ((int) countedMalloc, 0, "a malloc is counted");
                }
            }

            beginTest ("MasteringChain::processBlock does not allocate");
            {
                constexpr int blockSize = 512;
                MasteringChain chain;
                chain.prepare (kSampleRate, 2, blockSize);

                // An EQ curve to apply, set up off the audio thread as the plugin does.
                const auto reference = whiteNoise (2, samplesFor (2.0), 0.3f, 1u);
                auto dark = whiteNoise (2, samplesFor (2.0), 0.3f, 2u);
                chain.setReferenceTrack (reference, kSampleRate);
                chain.setCurrentTrackAnalysis (dark, kSampleRate);

                MasteringChain::Settings on;
                on.bypass = false;
                on.eqMatchAmount = 1.0f;
                MasteringChain::Settings off = on;
                off.bypass = true;

                auto input = whiteNoise (2, blockSize, 0.4f, 3u);
                for (int i = 0; i < 5; ++i) { chain.setSettings (on); auto b = input; chain.processBlock (b); } // warm up

                // AudioBuffer copies allocate, so the buffer is prepared outside the counted region and refilled in place.
                // Vary everything the plugin varies per block: bypass, target, compression, ceiling, EQ amount.
                juce::AudioBuffer<float> work (2, blockSize);
                startCountingAllocations();
                for (int block = 0; block < 400; ++block)
                {
                    auto s = (block % 100 < 20) ? off : on;
                    s.targetLoudnessLufs = -23.0f + (float) (block % 17);
                    s.compAmount = (float) (block % 11) / 10.0f;
                    s.limiterCeilingDb = -3.0f + (float) (block % 7) * 0.4f;
                    s.eqMatchAmount = (float) (block % 5) / 4.0f;
                    chain.setSettings (s);
                    for (int ch = 0; ch < 2; ++ch) juce::FloatVectorOperations::copy (work.getWritePointer (ch), input.getReadPointer (ch), blockSize);
                    chain.processBlock (work);
                }
                const size_t allocations = stopCountingAllocations();
                expectEquals ((int) allocations, 0, "400 blocks of 512 with settings, bypass and EQ changing every block: allocations");
                expect (allFinite (work), "and the output is finite");
            }

            beginTest ("the streaming meter and the trim do not allocate");
            {
                StreamingLoudnessMeter meter;
                meter.prepare (kSampleRate, 2);
                LoudnessTrim trim;
                trim.prepare (kSampleRate, 2);
                LookaheadLimiter limiter;
                limiter.prepare (kSampleRate, 2);
                auto work = whiteNoise (2, 512, 0.2f, 4u);
                const auto source = work;

                startCountingAllocations();
                for (int i = 0; i < 300; ++i)
                {
                    for (int ch = 0; ch < 2; ++ch) juce::FloatVectorOperations::copy (work.getWritePointer (ch), source.getReadPointer (ch), 512);
                    meter.process (work);
                    juce::ignoreUnused (meter.read (-50.0f));
                    trim.apply (work);
                    limiter.process (work);
                    trim.observeOutput (work);
                    limiter.delay (work);
                }
                const size_t pipelineAllocations = stopCountingAllocations();
                expectEquals ((int) pipelineAllocations, 0, "meter, trim and limiter over 300 blocks: allocations");

                LoudnessMeter shortTerm;
                shortTerm.prepare (kSampleRate, 2);
                startCountingAllocations();
                for (int i = 0; i < 100; ++i) juce::ignoreUnused (shortTerm.measureShortTermLoudness (work));
                const size_t shortTermAllocations = stopCountingAllocations();
                expectEquals ((int) shortTermAllocations, 0, "LoudnessMeter::measureShortTermLoudness: allocations");
            }
        }
    };

    static RealtimeSafetyTests realtimeSafetyTests;
}
