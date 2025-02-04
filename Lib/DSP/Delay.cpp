/*
  ==============================================================================

    Delay.cpp
    Created: 18 Mar 2023 2:45:26pm
    Author:  程

  ==============================================================================
*/

#include "Delay.h"

/* */
Delay::Delay()
{
    
}

/* */
Delay::~Delay()
{
    
}

/* */
void Delay::initialize(float inSampleRate, int inBlocksize)
{
    mSampleRate = inSampleRate;
    mCircularBuffer.setSize(1, 5 * inSampleRate);
    mTimeInSeconds.reset(inSampleRate, 0.25);
    mTimeInSeconds.setCurrentAndTargetValue(0.01);
    mGrainPitch.reset(inSampleRate, .01);
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = inSampleRate;
    spec.maximumBlockSize = inBlocksize;
    spec.numChannels = 1;
    
    mHighPassFilter.prepare(spec);
    mLowpassFilter.prepare(spec);
    
    mHighPassFilter.reset();
    mLowpassFilter.reset();
    
//    DBG("Real Time Grain Prepare To Play Called");
    mRealTimeGranulator.prepareToPlay(inSampleRate, inBlocksize);
}

/* */
void Delay::setParameters(float inTimeSeconds, float inFeedbackAmount, float inMix, float inLPFreq, float inHPFreq, float inGrainPitch)
{
    mTimeInSeconds.setTargetValue(inTimeSeconds);
    mFeedbackAmount = inFeedbackAmount;
    mMix = inMix;
    
//    mRealTimeGranulator.setParameters(inGrainSize);
    
    mHighPassFilter.coefficients = mHighpassCoefficients.makeHighPass(mSampleRate, inHPFreq);
    mLowpassFilter.coefficients = mLowpassCoefficients.makeLowPass(mSampleRate, inLPFreq);
    mGrainPitch.setTargetValue(inGrainPitch);
}

/* */
void Delay::processBlock(float* inBuffer, int inNumSamples)
{
    for (int i = 0; i < inNumSamples; i++) {
        processSample(inBuffer[i]);
    }
}

/* */
void Delay::processSample(float& inSample)
{
    mCircularBuffer.setSample(0, mWriteHead, std::tanh(inSample + (mFeedbackSample * mFeedbackAmount)));
    
    mWriteHead++;
    
    if (mWriteHead >= mCircularBuffer.getNumSamples()) {
        mWriteHead = 0;
    }
    
    float time_in_sample = mTimeInSeconds.getNextValue() * mSampleRate;
    float read_head = mWriteHead - time_in_sample;
    
    read_head = AudioHelpers::wrap_buffer(read_head, mCircularBuffer.getNumSamples());
    
    // Get Interpolation Positions
    int sample_x_pos = floor(read_head);
    int sample_x1_pos = sample_x_pos + 1;
    
    sample_x1_pos = AudioHelpers::wrap_buffer(sample_x1_pos, mCircularBuffer.getNumSamples());
    
    float inter_sample_amount = read_head - sample_x_pos;
    
    // Get Interpolation Values
    float sample_x = mCircularBuffer.getSample(0, sample_x_pos);
    float sample_x1 = mCircularBuffer.getSample(0, sample_x1_pos);
    float output_sample = AudioHelpers::lin_interp(sample_x, sample_x1, inter_sample_amount);
        
    output_sample = mHighPassFilter.processSample(output_sample);
    output_sample = mLowpassFilter.processSample(output_sample);
    
    mFeedbackSample = output_sample;
    
    mFeedbackSample = mRealTimeGranulator.processSample(mFeedbackSample, mGrainPitch.getNextValue());
        
    inSample = (output_sample * mMix) + (inSample * (1.f - mMix));
}

