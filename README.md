# loopy

A Multi-Eﬀects Delay/Looper on Bela
Jiawei Chong 2024

Loopy is an experimental audio looper with real-time effects, inspired by modern Microcosm-style pedals and modular audio practices. It records, plays, and overdubs sound from a high-sensitivity electret microphone, applying delay, feedback, and optional LFO-based modulation in real time.

2. Core Features
1. Recording & Overdub
•
Stores audio data in a ring buﬀer of around 20 seconds.
•
Supports overdub: new recordings can be mixed (+=) onto the existing buﬀer,
creating multi-layer loops.
2. Delay Eﬀect (DelayEﬀect)
•
Fixed or adjustable delay time, feedback, and mix.
•
Processes input signals during “recording,” and can be monitored in real time.
3. LFO Modulation
•
Uses a lfoparams structure (e.g., sine wave, etc.) to periodically modulate
certain parameters.
•
In the example, it modulates delay time, but can be applied elsewhere.
4. Playback Speed Control
•
Ranges from -2.0x to +2.0x playback speed, even reverse.
•
Allows instant pitch/rate changes and a wide variety of sonic transformations.
5. Two-Button Interaction
•
One button toggles “record/stop” states.
•
Another button provides a “one-touch clear” to reset the buﬀer and pointers.
6. LED Indicator
•
LED lights up when recording, goes oﬀ when paused or playing—helpful for
quick visual status during live use.
3. Technical Implementation
1. Bela Platform
•
Exploits Bela’s low-latency audio I/O and analog inputs, ensuring quick knob/
button response for real-time performance.
•
44.1 kHz sampling rate for high-quality audio processing.
2. Delay & Looper
•
A C++ DelayEﬀect class manages ring-buﬀer logic for delay, feedback, and
mix.
•
The recording buﬀer also uses a ring structure, storing ~20 seconds of audio
(adjustable if needed).
3. LFO
•
lfoparams with functions like init_lfo, run_lfo, etc., supporting multiple
waveforms (sine, triangle, exponential, integrated triangle, etc.).
•
The LFO’s output can be mapped to delay time or any other desired parameter.
4. Overdub Mixing
•
Uses += in the recording region to blend new input with existing material,
generating layered loops.
•
Real-time monitoring: you can hear the eﬀect (delay or others) immediately
while recording.
5. Playback Speed
•
A floating-point gPlaybackSpeed controls how fast readIndex increments. It
can be negative for reverse playback, or well above 1.0 for faster-than-normal speed, or
below 1.0 for slower.
6. Clear Logic
•
When the “clear” button is detected, the ring buﬀer is filled with zeros, read/
write pointers reset, and recording/playback halted, also resetting the LED.
4. Code Structure
•
render.cpp
•
DelayEﬀect, and lfoparams.
Contains the core looper and main flow. In setup(), it initializes I/O pins,
•
render() handles frame-by-frame recording, playback, eﬀect processing, and
monitoring.
•
•
Includes button checks and buﬀer clearing logic.
DelayEﬀect.h / DelayEﬀect.cpp
•
A self-contained delay-eﬀect class with methods like setDelayTime(),
setFeedback(), setMix().
•
Implements a simple ring buﬀer plus feedback for the delay eﬀect.
5. References & Inspiration
•
Bela’s oﬃcial multi-eﬀects examples and documentation at bela.io.
•
Modern looping delay devices like Microcosm, providing a blend of delay +
looping + modulation.
•
