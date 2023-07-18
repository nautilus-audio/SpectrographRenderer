//
//  Spectrograph.h
//  
//
//  Created by Alex Mitchell on 4/20/23.
//

#ifndef Spectrograph_h
#define Spectrograph_h

#include "WaveFile.h"

#endif /* Spectrograph_h */

class SpectrographRenderer : public juce::TimeSliceClient {
public:
    class Listener
    {
        
    public:
        virtual void OnComplete() = 0;
    };
    
    public:
        enum
        {
            fftOrder = 10,
            fftSize = 1 << fftOrder,
        };
    
        SpectrographRenderer(WaveFile::Ptr file, int blockSize);
    
        bool IsComplete() const { return complete; }
        const juce::Image& GetImage() const { return spectrographImage; }

        void AddListener(Listener* listener) { listeners.add(listener); }
        void RemoveListener(Listener* listener) { listeners.remove(listener); }
        void setImageSize(int newWidth, int newHeight);
        int getNumTimesToRunFFT();
        virtual int useTimeSlice() override;

protected:
    
    const float* ApplyFFT(const float* bufferData, const size_t numSamples, int channel);
    void drawNextBlockOfSpectrogram(int channel, const float *bufferChannel, int timesExecuted, int numSamplesToProcess);
    void doRender();
    
    juce::ListenerList<Listener> listeners;
    juce::Image spectrographImage;

    std::atomic<bool> complete;
    WaveFile::Ptr waveFile;
    juce::AudioFormatManager formatManager;
    juce::AudioFormatReader* formatReader;
    
    juce::dsp::FFT spectrographFFT{fftOrder};
    juce::dsp::WindowingFunction <float> spectrographWindow;
    
    juce::AudioBuffer<float> scopeData;
    int scopeSize = 0;
    float numTimesToRunFFT = 0.f;
    int numSamplesInBuffer;
    int spectroGraphBlockSize;
    float widthToCover;
    int lastXPosition, currentXPosition;
    bool endOfFile = false;
};
