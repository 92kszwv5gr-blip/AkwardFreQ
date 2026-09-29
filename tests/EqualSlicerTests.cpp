#include <juce_core/juce_core.h>
#include "separation/EqualSlicer.h"

namespace afq
{
    struct EqualSlicerTests : juce::UnitTest
    {
        EqualSlicerTests() : juce::UnitTest ("EqualSlicer", "AkwardFreQ") {}

        // True when the slices tile [from, to) with no gaps, overlaps or empty pieces, and are numbered 1..n.
        static bool tiles (const std::vector<DrumSlice>& s, int64_t from, int64_t to)
        {
            if (s.empty()) return from == to;
            if (s.front().startSample != from || s.back().endSample != to) return false;
            for (size_t i = 0; i < s.size(); ++i)
            {
                if (s[i].endSample <= s[i].startSample) return false;
                if (s[i].index != (int) i + 1) return false;
                if (i > 0 && s[i].startSample != s[i - 1].endSample) return false;
            }
            return true;
        }

        void runTest() override
        {
            beginTest ("splits evenly");
            {
                const auto s = EqualSlicer::slice (1000, 4);
                expectEquals ((int) s.size(), 4);
                expect (tiles (s, 0, 1000), "tiles the buffer");
                for (const auto& x : s) expectEquals ((int) (x.endSample - x.startSample), 250);
            }

            beginTest ("uneven lengths differ by at most one sample");
            {
                const auto s = EqualSlicer::slice (1001, 4);
                expect (tiles (s, 0, 1001), "tiles the buffer");
                int64_t shortest = 1 << 30, longest = 0;
                for (const auto& x : s) { shortest = std::min (shortest, x.endSample - x.startSample); longest = std::max (longest, x.endSample - x.startSample); }
                expectLessOrEqual ((int) (longest - shortest), 1);
            }

            beginTest ("slices only the requested range");
            {
                const auto s = EqualSlicer::slice (1000, 8, 100, 900);
                expectEquals ((int) s.size(), 8);
                expect (tiles (s, 100, 900), "tiles [100, 900)");
            }

            beginTest ("clamps its inputs");
            {
                expectEquals ((int) EqualSlicer::slice (1000, 0).size(), 1, "count 0 becomes 1");
                expectEquals ((int) EqualSlicer::slice (1000, -5).size(), 1, "negative count becomes 1");
                expectEquals ((int) EqualSlicer::slice (100000, 1000).size(), 256, "count is capped at 256");
                expect (tiles (EqualSlicer::slice (1000, 4, -50, 5000), 0, 1000), "range is clamped to the buffer");
                expect (tiles (EqualSlicer::slice (1000, 4, 0, -1), 0, 1000), "no end means the whole buffer");
                // The header comment says "the whole buffer"; the code slices from the start to the end of the buffer.
                expect (tiles (EqualSlicer::slice (1000, 4, 700, 300), 700, 1000), "end <= start means from the start to the end of the buffer");
                expect (EqualSlicer::slice (0, 4).empty(), "empty buffer gives no slices");
                expect (EqualSlicer::slice (1000, 4, 1000, 2000).empty() || tiles (EqualSlicer::slice (1000, 4, 1000, 2000), 0, 1000),
                        "a range that starts at the end does not crash");
            }

            beginTest ("tiles the range for many length and count combinations");
            {
                for (int64_t len : { 1, 2, 3, 7, 10, 100, 999, 1000, 44100 })
                    for (int count : { 1, 2, 3, 7, 16, 64, 256, 300 })
                    {
                        const auto s = EqualSlicer::slice (len, count);
                        const int expectedCount = (int) std::min<int64_t> (std::min (count, 256), len);
                        expect (tiles (s, 0, len), "tiles: len " + juce::String (len) + ", count " + juce::String (count));
                        expectEquals ((int) s.size(), expectedCount, "piece count: len " + juce::String (len) + ", count " + juce::String (count));
                    }
            }
        }
    };

    static EqualSlicerTests equalSlicerTests;
}
