#ifndef DELAY_EFFECT_H
#define DELAY_EFFECT_H

#include <vector>
#include <algorithm>

template <typename T>
T clampValue(T value, T minVal, T maxVal) {
    if(value < minVal) return minVal;
    if(value > maxVal) return maxVal;
    return value;
}

class DelayEffect {
public:
    DelayEffect(unsigned int sr, float delayTimeSec, float feedbackAmount, unsigned int bufSize);

    void setDelayTime(float delayTimeSec);
    void setFeedback(float feedbackAmount);
    void setMix(float mixAmount);

    // optional smoothing factor if you want it
    void setTimeSmoothingFactor(float factor) { timeSmoothingFactor = clampValue(factor, 0.f, 1.f); }

    float processSample(float inputSample);

private:
    unsigned int sampleRate;
    unsigned int bufferSize;
    unsigned int writePointer;

    // new members:
    float currentDelayTimeInSamples;
    float targetDelayTimeInSamples;
    float timeSmoothingFactor;

    // old parameters:
    float feedback; 
    float mix;

    // ring buffer
    std::vector<float> delayBuffer;
};

#endif