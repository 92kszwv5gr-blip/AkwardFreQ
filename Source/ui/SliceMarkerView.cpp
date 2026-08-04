#include "SliceMarkerView.h"

namespace afq
{
    SliceMarkerView::SliceMarkerView() { setWantsKeyboardFocus (false); }

    void SliceMarkerView::setAudioSource (const juce::AudioBuffer<float>* displayBuffer, double sampleRate)
    {
        buffer_ = displayBuffer;
        sampleRate_ = sampleRate;
        repaint();
    }

    void SliceMarkerView::setSlices (std::vector<DrumSlice> slices)
    {
        slices_ = std::move (slices);
        if (selectedIndex_ >= (int) slices_.size()) selectedIndex_ = -1;
        repaint();
    }

    void SliceMarkerView::setSelectedSliceIndex (int index) { selectedIndex_ = index; repaint(); }

    float SliceMarkerView::sampleToX (int64_t sample) const
    {
        if (buffer_ == nullptr || buffer_->getNumSamples() <= 0) return 0.0f;
        return (float) getWidth() * (float) sample / (float) buffer_->getNumSamples();
    }

    int64_t SliceMarkerView::xToSample (float x) const
    {
        if (buffer_ == nullptr || getWidth() <= 0) return 0;
        return (int64_t) ((double) x / (double) getWidth() * (double) buffer_->getNumSamples());
    }

    int SliceMarkerView::findSliceAt (int64_t sample) const
    {
        for (int i = 0; i < (int) slices_.size(); ++i)
            if (sample >= slices_[(size_t) i].startSample && sample < slices_[(size_t) i].endSample)
                return i;
        return -1;
    }

    void SliceMarkerView::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff1a1a1a));

        const int width = getWidth();
        const int height = getHeight();
        const float midY = height * 0.5f;

        if (buffer_ != nullptr && buffer_->getNumSamples() > 0)
        {
            g.setColour (juce::Colour (0xff5dade2));
            const int numChannels = buffer_->getNumChannels();
            const int totalSamples = buffer_->getNumSamples();

            for (int px = 0; px < width; ++px)
            {
                const int64_t s0 = (int64_t) ((double) px / width * totalSamples);
                const int64_t s1 = juce::jmax (s0 + 1, (int64_t) ((double) (px + 1) / width * totalSamples));
                const int64_t clampedS1 = juce::jmin (s1, (int64_t) totalSamples);
                if (clampedS1 <= s0) continue;

                float peak = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto range = juce::FloatVectorOperations::findMinAndMax (
                        buffer_->getReadPointer (ch) + s0, (int) (clampedS1 - s0));
                    peak = juce::jmax (peak, std::abs (range.getStart()), std::abs (range.getEnd()));
                }
                const float y = peak * midY;
                g.drawVerticalLine (px, midY - y, midY + y);
            }
        }

        // Slice markers: alternating translucent fills so adjacent slices are
        // visually distinguishable even when all the same "color" (unlike the
        // Split tab, slices here aren't layer-classified, so there's no
        // per-slice semantic color to draw on).
        for (int i = 0; i < (int) slices_.size(); ++i)
        {
            const auto& s = slices_[(size_t) i];
            const float x0 = sampleToX (s.startSample);
            const float x1 = sampleToX (s.endSample);
            const bool selected = (i == selectedIndex_);

            g.setColour (juce::Colour (0xffffffff).withAlpha (selected ? 0.16f : (i % 2 == 0 ? 0.05f : 0.09f)));
            g.fillRect (juce::Rectangle<float> (x0, 0.0f, juce::jmax (1.0f, x1 - x0), (float) height));

            g.setColour (selected ? juce::Colour (0xff5dade2) : juce::Colours::white.withAlpha (0.5f));
            g.drawVerticalLine ((int) x0, 0.0f, (float) height);

            if (x1 - x0 > 16.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.setFont (10.0f);
                g.drawText (juce::String (s.index), (int) x0 + 2, 2, (int) (x1 - x0) - 4, 12,
                            juce::Justification::centredLeft, true);
            }
        }
    }

    void SliceMarkerView::resized() {}

    void SliceMarkerView::mouseDown (const juce::MouseEvent& e)
    {
        const int64_t sample = xToSample ((float) e.x);
        const int index = findSliceAt (sample);
        setSelectedSliceIndex (index);
        if (onSliceSelected) onSliceSelected (index);
    }
}
