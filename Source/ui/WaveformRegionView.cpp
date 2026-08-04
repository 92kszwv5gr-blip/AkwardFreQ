#include "WaveformRegionView.h"
#include <cmath>

namespace afq
{
    namespace
    {
        constexpr float kEdgeHandlePixels = 6.0f;
        constexpr int64_t kMinRegionSamples = 64;
    }

    WaveformRegionView::WaveformRegionView() { setWantsKeyboardFocus (false); }

    void WaveformRegionView::setAudioSource (const juce::AudioBuffer<float>* displayBuffer, double sampleRate)
    {
        buffer_ = displayBuffer;
        sampleRate_ = sampleRate;
        totalLengthSamples_ = buffer_ != nullptr ? buffer_->getNumSamples() : 0;
        if (viewLength_ == 0) setViewRange (0, totalLengthSamples_);
        repaint();
    }

    void WaveformRegionView::setRegions (std::vector<Region>* regions) { regions_ = regions; repaint(); }
    void WaveformRegionView::refresh() { repaint(); }

    void WaveformRegionView::setViewRange (int64_t startSample, int64_t lengthSamples)
    {
        viewStart_ = juce::jlimit ((int64_t) 0, juce::jmax ((int64_t) 0, totalLengthSamples_ - 1), startSample);
        viewLength_ = juce::jmax ((int64_t) 1, lengthSamples);
        repaint();
    }

    void WaveformRegionView::setSelectedRegionIndex (int index) { selectedIndex_ = index; repaint(); }

    float WaveformRegionView::sampleToX (int64_t sample) const
    {
        if (viewLength_ <= 0) return 0.0f;
        return (float) getWidth() * (float) (sample - viewStart_) / (float) viewLength_;
    }

    int64_t WaveformRegionView::xToSample (float x) const
    {
        if (getWidth() <= 0) return viewStart_;
        return viewStart_ + (int64_t) ((double) x / (double) getWidth() * (double) viewLength_);
    }

    int WaveformRegionView::findRegionAt (int64_t sample) const
    {
        if (regions_ == nullptr) return -1;
        for (int i = 0; i < (int) regions_->size(); ++i)
        {
            const auto& r = (*regions_)[(size_t) i];
            if (sample >= r.startSample && sample < r.endSample) return i;
        }
        return -1;
    }

    WaveformRegionView::DragMode WaveformRegionView::edgeKindAt (int regionIndex, float x, float pixelTolerance) const
    {
        if (regionIndex < 0 || regions_ == nullptr || regionIndex >= (int) regions_->size()) return DragMode::none;
        const auto& r = (*regions_)[(size_t) regionIndex];
        if (std::abs (x - sampleToX (r.startSample)) <= pixelTolerance) return DragMode::moveStart;
        if (std::abs (x - sampleToX (r.endSample)) <= pixelTolerance) return DragMode::moveEnd;
        return DragMode::none;
    }

    int WaveformRegionView::findEdgeHandleAt (float x, float pixelTolerance) const
    {
        if (regions_ == nullptr) return -1;
        for (int i = 0; i < (int) regions_->size(); ++i)
            if (edgeKindAt (i, x, pixelTolerance) != DragMode::none)
                return i;
        return -1;
    }

    void WaveformRegionView::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff1a1a1a));

        const int width = getWidth();
        const int height = getHeight();
        const float midY = height * 0.5f;

        // Waveform envelope.
        if (buffer_ != nullptr && buffer_->getNumSamples() > 0 && viewLength_ > 0)
        {
            g.setColour (juce::Colour (0xff5dade2));
            juce::Path path;
            path.startNewSubPath (0.0f, midY);

            const int numChannels = buffer_->getNumChannels();
            for (int px = 0; px < width; ++px)
            {
                const int64_t s0 = viewStart_ + (int64_t) ((double) px / width * viewLength_);
                const int64_t s1 = viewStart_ + (int64_t) ((double) (px + 1) / width * viewLength_);
                const int64_t clampedS0 = juce::jlimit ((int64_t) 0, totalLengthSamples_, s0);
                const int64_t clampedS1 = juce::jlimit (clampedS0 + 1, totalLengthSamples_, s1);

                float peak = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto range = juce::FloatVectorOperations::findMinAndMax (
                        buffer_->getReadPointer (ch) + clampedS0, (int) (clampedS1 - clampedS0));
                    peak = juce::jmax (peak, std::abs (range.getStart()), std::abs (range.getEnd()));
                }
                const float y = peak * midY;
                g.drawVerticalLine (px, midY - y, midY + y);
            }
        }

        // Region overlays.
        if (regions_ != nullptr)
        {
            for (int i = 0; i < (int) regions_->size(); ++i)
            {
                const auto& r = (*regions_)[(size_t) i];
                const float x0 = sampleToX (r.startSample);
                const float x1 = sampleToX (r.endSample);
                if (x1 < 0.0f || x0 > (float) width) continue;

                auto colour = layerColour (r.type);
                g.setColour (colour.withAlpha (i == selectedIndex_ ? 0.45f : 0.22f));
                g.fillRect (juce::Rectangle<float> (x0, 0.0f, juce::jmax (1.0f, x1 - x0), (float) height));

                g.setColour (colour.withAlpha (i == selectedIndex_ ? 1.0f : 0.6f));
                g.drawVerticalLine ((int) x0, 0.0f, (float) height);

                if (x1 - x0 > 24.0f)
                {
                    g.setFont (11.0f);
                    g.drawText (layerName (r.type), (int) x0 + 2, 2, (int) (x1 - x0) - 4, 14,
                                juce::Justification::centredLeft, true);
                }
            }
        }
    }

    void WaveformRegionView::resized() {}

    void WaveformRegionView::mouseMove (const juce::MouseEvent& e)
    {
        const bool overEdge = findEdgeHandleAt ((float) e.x, kEdgeHandlePixels) >= 0;
        setMouseCursor (overEdge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }

    void WaveformRegionView::mouseDown (const juce::MouseEvent& e)
    {
        const int edgeRegion = findEdgeHandleAt ((float) e.x, kEdgeHandlePixels);
        if (edgeRegion >= 0)
        {
            dragRegionIndex_ = edgeRegion;
            dragMode_ = edgeKindAt (edgeRegion, (float) e.x, kEdgeHandlePixels);
            setSelectedRegionIndex (edgeRegion);
            if (onRegionSelected) onRegionSelected (edgeRegion);
            return;
        }

        dragMode_ = DragMode::none;
        const int64_t sample = xToSample ((float) e.x);
        const int region = findRegionAt (sample);
        setSelectedRegionIndex (region);
        if (onRegionSelected) onRegionSelected (region);
    }

    void WaveformRegionView::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragMode_ == DragMode::none || dragRegionIndex_ < 0 || regions_ == nullptr) return;
        if (dragRegionIndex_ >= (int) regions_->size()) return;

        auto& r = (*regions_)[(size_t) dragRegionIndex_];
        const int64_t sample = juce::jlimit ((int64_t) 0, totalLengthSamples_, xToSample ((float) e.x));

        if (dragMode_ == DragMode::moveStart)
            r.startSample = juce::jmin (sample, r.endSample - kMinRegionSamples);
        else
            r.endSample = juce::jmax (sample, r.startSample + kMinRegionSamples);

        repaint();
    }

    void WaveformRegionView::mouseUp (const juce::MouseEvent&)
    {
        if (dragMode_ != DragMode::none && regions_ != nullptr && dragRegionIndex_ >= 0
            && dragRegionIndex_ < (int) regions_->size())
        {
            (*regions_)[(size_t) dragRegionIndex_].userCorrected = true;
            if (onRegionsChanged) onRegionsChanged();
        }
        dragMode_ = DragMode::none;
        dragRegionIndex_ = -1;
    }
}
