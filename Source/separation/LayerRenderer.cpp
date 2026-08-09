#include "LayerRenderer.h"

namespace afq
{
    namespace
    {
        constexpr float kFadeMs = 5.0f;

        void applySingleFilter (juce::AudioBuffer<float>& buffer, double sampleRate,
                                 juce::dsp::IIR::Coefficients<float>::Ptr coeffs)
        {
            juce::dsp::IIR::Filter<float> filter;
            filter.coefficients = coeffs;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                filter.reset();
                juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers() + ch, 1, (size_t) buffer.getNumSamples());
                juce::dsp::ProcessContextReplacing<float> ctx (block);
                filter.process (ctx);
            }
            juce::ignoreUnused (sampleRate);
        }
    }

    void LayerRenderer::applyTypeFilter (juce::AudioBuffer<float>& buffer, double sampleRate, LayerType type)
    {
        using Coeffs = juce::dsp::IIR::Coefficients<float>;

        switch (type)
        {
            case LayerType::Kick:
                applySingleFilter (buffer, sampleRate, Coeffs::makeLowPass (sampleRate, 180.0));
                break;
            case LayerType::Bass:
                applySingleFilter (buffer, sampleRate, Coeffs::makeLowPass (sampleRate, 250.0));
                break;
            case LayerType::HiHat:
                applySingleFilter (buffer, sampleRate, Coeffs::makeHighPass (sampleRate, 5000.0));
                break;
            case LayerType::Percussion:
                applySingleFilter (buffer, sampleRate, Coeffs::makeHighPass (sampleRate, 150.0));
                applySingleFilter (buffer, sampleRate, Coeffs::makeLowPass (sampleRate, 6000.0));
                break;
            case LayerType::Breakbeat:
                applySingleFilter (buffer, sampleRate, Coeffs::makeHighPass (sampleRate, 80.0));
                break;
            case LayerType::SynthLead:
                applySingleFilter (buffer, sampleRate, Coeffs::makeHighPass (sampleRate, 150.0));
                applySingleFilter (buffer, sampleRate, Coeffs::makeLowPass (sampleRate, 5000.0));
                break;
            case LayerType::Stabs:
                applySingleFilter (buffer, sampleRate, Coeffs::makeHighPass (sampleRate, 150.0));
                break;
            case LayerType::Atmosphere:
                applySingleFilter (buffer, sampleRate, Coeffs::makeHighPass (sampleRate, 80.0));
                break;
            case LayerType::Zap:
                applySingleFilter (buffer, sampleRate, Coeffs::makeHighPass (sampleRate, 200.0));
                break;
            case LayerType::Glitch:
                applySingleFilter (buffer, sampleRate, Coeffs::makeHighPass (sampleRate, 300.0));
                break;
            case LayerType::FX:
            case LayerType::Unclassified:
            default:
                break; // broadband, no filtering
        }
    }

    std::array<juce::AudioBuffer<float>, (size_t) LayerType::Count>
        LayerRenderer::render (const juce::AudioBuffer<float>& sourceBuffer, double sampleRate,
                                const std::vector<Region>& regions)
    {
        std::array<juce::AudioBuffer<float>, (size_t) LayerType::Count> output;
        const int numChannels = sourceBuffer.getNumChannels();
        const int numSamples = sourceBuffer.getNumSamples();
        const int fadeSamples = juce::jmax (1, (int) (sampleRate * kFadeMs / 1000.0));

        std::array<bool, (size_t) LayerType::Count> touched {};
        touched.fill (false);

        for (const auto& region : regions)
        {
            const size_t typeIdx = (size_t) region.type;
            auto& dst = output[typeIdx];
            if (! touched[typeIdx])
            {
                dst.setSize (numChannels, numSamples, false, true, true);
                dst.clear();
                touched[typeIdx] = true;
            }

            const int start = juce::jlimit (0, numSamples, (int) region.startSample);
            const int end = juce::jlimit (0, numSamples, (int) region.endSample);
            const int len = end - start;
            if (len <= 0) continue;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* src = sourceBuffer.getReadPointer (ch);
                float* out = dst.getWritePointer (ch);
                for (int i = 0; i < len; ++i)
                {
                    float gain = 1.0f;
                    if (i < fadeSamples) gain = (float) i / (float) fadeSamples;
                    else if (i >= len - fadeSamples) gain = (float) (len - 1 - i) / (float) fadeSamples;
                    out[start + i] += src[start + i] * gain;
                }
            }
        }

        for (size_t i = 0; i < (size_t) LayerType::Count; ++i)
            if (touched[i])
                applyTypeFilter (output[i], sampleRate, (LayerType) i);

        return output;
    }
}
