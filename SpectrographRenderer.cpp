/*
  ==============================================================================

    SpectrographRenderer.cpp
    Created: 20 Apr 2023 11:41:10am
    Author:  Alex Mitchell

  ==============================================================================
*/

#include "SpectrographRenderer.h"

using namespace juce;

SpectrographRenderer::SpectrographRenderer(WaveFile::Ptr file, int blockSize) : waveFile(file), spectrographWindow(spectrographFFT.getSize() + 1, juce::dsp::WindowingFunction < float >::hann, false)
{
    
    formatManager.registerBasicFormats();
    waveFile = file;
    spectroGraphBlockSize = blockSize * 2;
    
    // read from file
    auto activeReaderSource = file->CreateReaderSource(formatManager);
    if (activeReaderSource)
    {
        formatReader = activeReaderSource->getAudioFormatReader();
        int numChannels = formatReader->numChannels;
        scopeData.setSize(numChannels, spectroGraphBlockSize);
    }
    
    auto totalNumSamples = formatReader->lengthInSamples;
    numTimesToRunFFT = std::ceil(totalNumSamples / spectroGraphBlockSize);
    complete = false;
}

int SpectrographRenderer::useTimeSlice()
{
        
    auto activeReaderSource = waveFile->CreateReaderSource(formatManager);
    if (activeReaderSource)
    {
        formatReader = activeReaderSource->getAudioFormatReader();
        int numChannels = formatReader->numChannels;
        scopeData.setSize(numChannels, spectroGraphBlockSize);
    }
    
    if (!complete)
    {
        doRender();
    }
    constexpr int timeUntilNextTimeslice = 0;
    return timeUntilNextTimeslice;
}

void SpectrographRenderer::doRender()
{
    int sampleIndex = 0;
    auto totalNumSamples = formatReader->lengthInSamples;
    auto numSamplesLeft = totalNumSamples;
    const int numChannels = scopeData.getNumChannels();
    numSamplesInBuffer = scopeData.getNumSamples();
    int timesExecuted = 1;
    
    // process one large FFT block size here (e.g. 4096 samples)....
    int numSamplesToRead = spectroGraphBlockSize;
    
    AudioBuffer<float> summedBuffer(1, scopeData.getNumSamples());
    
    while(numSamplesLeft > 0)
    {
        if(numSamplesLeft < spectroGraphBlockSize)
        {
            numSamplesToRead = static_cast<int>(numSamplesLeft);
        }
        
        for(int channel = 0; channel < numChannels; channel++)
        {
            AudioBuffer<float> singleChannel(1, scopeData.getNumSamples());
            singleChannel.copyFrom(0, 0, scopeData, channel, 0, numSamplesToRead);
        
            // Read samples from wav file
            formatReader->read(&singleChannel, 0, numSamplesToRead, sampleIndex, true, true);
            
            summedBuffer.copyFrom(0, 0, singleChannel, 0, 0, numSamplesToRead);
        }
        auto summedBufferArray = summedBuffer.getReadPointer(0);

        // render pixels to image
        drawNextBlockOfSpectrogram(0, summedBufferArray, timesExecuted, numSamplesToRead);
        
        sampleIndex += numSamplesToRead;
        numSamplesLeft -= numSamplesToRead;
                        
        if(numSamplesLeft == 0)
        {
            endOfFile = true;
        }
        
        timesExecuted++;
    }

    //    when file is run, end == true
    if (endOfFile)
    {
        complete = true;
        listeners.call([this](Listener& l) { l.OnComplete(); });
    }
}

const float* SpectrographRenderer::ApplyFFT(const float* bufferData, const size_t numSamples, int channel)
{
    unsigned long int UL = 1;
    const int fftSize = spectrographFFT.getSize();
    std::vector<float> fftBuffer(fftSize * (2 * UL));
        
    // ...copy the frequency information from fftBuffer to the spectrum
    std::memcpy(fftBuffer.data(), bufferData, fftSize * sizeof(float));
    
    // Apply Windowing function
    spectrographWindow.multiplyWithWindowingTable(fftBuffer.data(), fftSize);
    
    // Spectrograms are created by extracting that frequency data from an audio signal. We are not modifying the data, so no need for an iFFT.
    spectrographFFT.performFrequencyOnlyForwardTransform(fftBuffer.data());
    
    std::memcpy(scopeData.getWritePointer(channel), fftBuffer.data(), fftSize * sizeof(float));
    
    scopeData.copyFrom(channel, 0, fftBuffer.data(), static_cast<int>(numSamples));
    
    return scopeData.getReadPointer(channel);
}

void SpectrographRenderer::drawNextBlockOfSpectrogram(int channel, const float *bufferChannel, int timesExecuted, int numSamplesToProcess)
{
    auto imageHeight = spectrographImage.getHeight();
    float imageWidth = spectrographImage.getWidth() - 1;
        
    float widthFraction = numSamplesToProcess / numSamplesInBuffer;
    
    float widthOfImageToCoverThisRun =  float(imageWidth / numTimesToRunFFT);
    widthToCover = std::ceil(widthOfImageToCoverThisRun * widthFraction);
    
    // first, shuffle our image leftwards by 1 pixel..
    spectrographImage.moveImageSection (0, 0, 1, 0, widthToCover, imageHeight);
        
    // do fft
    auto scopeDataChannel = ApplyFFT(bufferChannel, fftSize, channel);

    // find the range of values produced, so we can scale our rendering to
    // show up the detail clearly
    auto maxLevel = juce::FloatVectorOperations::findMinAndMax (scopeData.getReadPointer(channel), fftSize / 2);
    
    
    for (auto y = 1; y < imageHeight; ++y)
    {
        auto skewedProportionY = 1.0f - std::exp (std::log ((float) y / (float) imageHeight) * 0.2f);
        auto scopeDataIndex = (size_t) juce::jlimit (0, (int) fftSize / 2, (int) (skewedProportionY * fftSize / 2));
        auto level = juce::jmap (scopeDataChannel[scopeDataIndex], 0.0f, juce::jmax (maxLevel.getEnd(), 1e-5f), 0.0f, 1.0f);
        auto color = juce::Colour::fromHSV (level, 1.0f, level, 1.0f);
        
        if(currentXPosition != lastXPosition)
        {
            spectrographImage.setPixelAt (currentXPosition, y, color);
        }
    }
    
    currentXPosition = lastXPosition;
    lastXPosition += widthToCover;
}

void SpectrographRenderer::setImageSize(int newWidth, int newHeight)
{
    auto imageType = SoftwareImageType();
    spectrographImage = Image(juce::Image::RGB, newWidth, newHeight, true, imageType);
}

int SpectrographRenderer::getNumTimesToRunFFT()
{
    return numTimesToRunFFT;
}

