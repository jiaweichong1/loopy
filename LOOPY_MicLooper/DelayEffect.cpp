#include "DelayEffect.h"
#include <cmath>   // floor, etc.

DelayEffect::DelayEffect(unsigned int sr, float delayTimeSec, float feedbackAmount, unsigned int bufSize)
    : sampleRate(sr)
    , bufferSize(bufSize)
    , writePointer(0)
    , currentDelayTimeInSamples(0.0f)
    , targetDelayTimeInSamples(0.0f)
    , timeSmoothingFactor(0.01f) // default
    , feedback(0.5f)
    , mix(0.5f)
{
    delayBuffer.resize(bufferSize, 0.0f);

    setDelayTime(delayTimeSec); // sets targetDelayTimeInSamples
    currentDelayTimeInSamples = targetDelayTimeInSamples;

    setFeedback(feedbackAmount);
    setMix(0.5f);
}

void DelayEffect::setDelayTime(float delayTimeSec) {
    unsigned int samples = (unsigned int)(delayTimeSec * sampleRate);
    samples = std::min(samples, bufferSize - 1);
    targetDelayTimeInSamples = (float)samples;
}

void DelayEffect::setFeedback(float feedbackAmount) {
    feedback = clampValue(feedbackAmount, 0.f, 1.f);
}

void DelayEffect::setMix(float mixAmount) {
    mix = clampValue(mixAmount, 0.f, 1.f);
}


float DelayEffect::processSample(float inputSample)
{
    // smooth transitions:
    float diff = targetDelayTimeInSamples - currentDelayTimeInSamples;
    currentDelayTimeInSamples += timeSmoothingFactor * diff;

    // ring buffer read position
    float desiredRead = (float)writePointer - currentDelayTimeInSamples;
    while(desiredRead < 0.f) desiredRead += (float)bufferSize;
    while(desiredRead >= (float)bufferSize) desiredRead -= (float)bufferSize;

    int floorPos = (int)std::floor(desiredRead);
    float frac = desiredRead - (float)floorPos;
    int nextPos = (floorPos + 1) % bufferSize;

    float delayedSample = (1.f - frac)*delayBuffer[floorPos] + frac*delayBuffer[nextPos];

    float output = (1.f - mix)*inputSample + mix*delayedSample;

    // write
    delayBuffer[writePointer] = inputSample + delayedSample * feedback;
    writePointer = (writePointer + 1) % bufferSize;

    return output;
}