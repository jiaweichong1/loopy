     /*
  Loopy: A multi-effect delay/looper project on Bela
  -------------------------------------------------
  This code implements a looper that records audio from a high-sensitivity electret
  microphone, applies a delay effect (DelayEffect.h / DelayEffect.cpp), and optionally
  modulates the delay time via an LFO (lfo.h / lfo.cpp). The user can overdub multiple
  layers into a ring buffer (up to 10–20 seconds), while real-time monitoring is enabled.

  Key features:
    - Four analog knobs:
      1) Delay Mix (analog0)
      2) Delay Feedback (analog1)
      3) LFO Depth (analog2) -> how strongly LFO modulates delay time
      4) Playback Speed (analog3): [-2.0..+2.0]
    - Two digital buttons:
      - Record/Play toggle
      - Clear buffer (reset all loops)
    - LED indicator (digital output):
      - ON when recording, OFF when paused/playing

  Enjoy exploring or customizing this code for your own audio experiments.
*/

#include <Bela.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include "DelayEffect.h"
#include "lfo.h"

// ------------------------------------------------------
// Global state: ring buffer for audio, pointers, etc.
std::vector<float> gAudioBuffer;
int gBufferSize = 44100 * 20; // up to ~20 sec or more
int gWritePointer = 0;
int gReadPointer  = 0;
unsigned int gAudioFramesPerAnalogFrame = 0;
static float readIndex = 0.0f; // playback pointer

// Recording/playback states
bool gRecording      = false;
bool gPlaying        = false;
bool gClearBuffer    = false;
bool gClearedOnce    = false;
float gPlaybackSpeed = 1.0f;  

// Pin definitions
int gButtonPin       = 7;    // record/play button
int gClearButtonPin  = 10;   // clear buffer button
int gLEDPin          = 6;    // LED indicator
int gLastButtonState = 0;
int gLastClearButtonState = 0;

// Analog inputs
//  - analog0: Delay Mix
//  - analog1: Delay Feedback
//  - analog2: LFO Depth => modulates delay time
//  - analog3: Playback Speed => [-2..+2]
int gAnalogDelayMixChannel    = 0;
int gAnalogFeedbackChannel    = 1;
int gAnalogLfoDepthChannel    = 2;
int gAnalogSpeedChannel       = 3;

// DelayEffect instance with initial parameters
DelayEffect delayEffect(44100, 0.5f, 0.7f, 44100);

// LFO pointer for modulating delay time
lfoparams* gLFO = nullptr;

// ------------------------------------------------------
// Setup runs once before audio processing begins
bool setup(BelaContext *context, void *userData)
{
    // Determine ratio of audioFrames to analogFrames
    if(context->analogFrames)
        gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;

    // Initialize the looper buffer to zero
    gAudioBuffer.resize(gBufferSize, 0.0f);

    // Configure digital pins for buttons & LED
    pinMode(context, 0, gButtonPin, INPUT);
    pinMode(context, 0, gLEDPin, OUTPUT);
    pinMode(context, 0, gClearButtonPin, INPUT);

    // Initialize LFO at e.g. 0.1Hz or 1Hz
    float fs = context->audioSampleRate;
    gLFO = init_lfo(gLFO, 0.1f, fs, 0.0f); // frequency=0.1Hz for slow sweep
    set_lfo_type(gLFO, SINE);

    rt_printf("Looper + Delay + Overdub + LFO => (DelayTime + PlaybackSpeed)\n");
    return true;
}

// ------------------------------------------------------
// Render is called each audio frame
void render(BelaContext *context, void *userData)
{
    for(unsigned int n = 0; n < context->audioFrames; n++)
    {
        // Every gAudioFramesPerAnalogFrame frames, read the analog knobs
        if(gAudioFramesPerAnalogFrame && !(n % gAudioFramesPerAnalogFrame))
        {
            // (1) Delay Mix
            float analogMix = analogRead(context, n / gAudioFramesPerAnalogFrame, gAnalogDelayMixChannel);
            delayEffect.setMix(analogMix);

            // (2) Delay Feedback
            float analogFb = analogRead(context, n / gAudioFramesPerAnalogFrame, gAnalogFeedbackChannel);
            delayEffect.setFeedback(analogFb);

            // (3) Playback Speed => [-2..+2]
            float speedVal = analogRead(context, n / gAudioFramesPerAnalogFrame, gAnalogSpeedChannel);
            gPlaybackSpeed = -2.0f + speedVal * 4.0f; 

            // (4) LFO Depth => how strongly LFO modulates delay time
            float lfoDepth = analogRead(context, n / gAudioFramesPerAnalogFrame, gAnalogLfoDepthChannel);
            if(lfoDepth < 0.0f) lfoDepth = 0.0f;
            if(lfoDepth > 1.0f) lfoDepth = 1.0f;

            // Run LFO, returns ~[0..1]
            float lfoVal = run_lfo(gLFO);

            // Map LFO output [0..1] => [-1..+1]
            float mod = (lfoVal - 0.5f) * 2.0f;

            // We want e.g. base=0.1s, maxDelta=1.9 => final range [0.1-1.9..0.1+1.9] => [~0.0..2.0]
            // We'll clamp to [0.01..2.0] in case it goes beyond
            float baseDelaySec  = 0.1f;   // baseline
            float maxDelta      = 1.9f;   // how far above/below base we can go

            // Then the actual offset = mod * maxDelta * lfoDepth => [-1.9..+1.9]*lfoDepth
            float offset = mod * maxDelta * lfoDepth;
            float newDelayTime = baseDelaySec + offset; // might be from ~(-1.8) up to 2.0 or so

            // clamp [0.01..2.0]
            if(newDelayTime < 0.01f) newDelayTime = 0.01f;
            if(newDelayTime > 2.0f)  newDelayTime = 2.0f;

            // Apply to DelayEffect
            delayEffect.setDelayTime(newDelayTime);
        }

        // Read the audio input
        float in = audioRead(context, n, 0);

        // Check buttons
        int buttonState      = digitalRead(context, n, gButtonPin);
        int clearButtonState = digitalRead(context, n, gClearButtonPin);

        // (A) Record/Play toggle
        static int localLastButtonState = 0;
        if(buttonState == 1 && localLastButtonState == 0)
        {
            if(gRecording)
            {
                gRecording = false;
                gPlaying   = true;
                digitalWrite(context, n, gLEDPin, LOW);
            }
            else
            {
                gRecording = true;
                gPlaying   = true;
                digitalWrite(context, n, gLEDPin, HIGH);
            }
        }
        localLastButtonState = buttonState;

        // (B) Clear buffer
        static int localLastClearButtonState = 0;
        if(clearButtonState == 1 && localLastClearButtonState == 0)
        {
            if(!gClearedOnce)
            {
                std::fill(gAudioBuffer.begin(), gAudioBuffer.end(), 0.0f);
                gWritePointer = 0;
                gReadPointer  = 0;
                readIndex     = 0.0f;
                gRecording    = false;
                gPlaying      = false;
                digitalWrite(context, n, gLEDPin, LOW);
                gClearedOnce  = true;
            }
        }
        if(clearButtonState == 0 && localLastClearButtonState == 1)
        {
            gClearedOnce = false;
        }
        localLastClearButtonState = clearButtonState;

        // Processing: rec or play
        float out = 0.0f;

        // If Recording => pass input through DelayEffect => Overdub
        if(gRecording)
        {
            float processedIn = delayEffect.processSample(in);
            out += processedIn; // real-time monitor
            gAudioBuffer[gWritePointer] += (processedIn * 0.75f);
            gWritePointer = (gWritePointer + 1) % gBufferSize;
        }

        // If Playing => read from loop buffer
        if(gPlaying)
        {
            float playSample = gAudioBuffer[(int)readIndex % gBufferSize];
            out += playSample;

            // update readIndex by gPlaybackSpeed
            readIndex += gPlaybackSpeed;
            if(readIndex < 0)
                readIndex += gBufferSize;
            else if(readIndex >= gBufferSize)
                readIndex -= gBufferSize;
        }

        // Output final to both channels
        for(unsigned int channel = 0; channel < context->audioOutChannels; channel++)
        {
            audioWrite(context, n, channel, out);
        }
    }
}

// ------------------------------------------------------
// Cleanup runs once after audio has stopped
void cleanup(BelaContext *context, void *userData)
{
    rt_printf("Looper cleanup done.\n");
    // e.g. if LFO allocated with malloc, free(gLFO);
    // free(gLFO);
}