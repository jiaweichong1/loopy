/*
  lfo.cpp: Provides various Low-Frequency Oscillator (LFO) implementations
  for real-time audio applications. This includes integrated triangle,
  triangle, sine, square (compressed sine), exponential, and RC relaxation
  oscillator variants. The code defines functions to initialize an LFO,
  update its frequency, and run the selected LFO type each frame.

  Typical usage:
    1) Call init_lfo() to allocate and configure lfoparams, specifying
       base frequency (fosc), sample rate (fs), and initial phase.
    2) Optionally call update_lfo() to change the LFO rate at runtime.
    3) Call run_lfo() each audio frame (or as needed) to get a normalized
       oscillator value in [0,1], which can be mapped to target parameters
       (e.g. delay time, feedback, pitch).

  The lfoparams structure stores state variables for each oscillator type
 

*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "lfo.h"

/*
  init_lfo():
    Allocates an lfoparams struct and initializes it based on:
      - fosc: base LFO rate (in Hz),
      - fs:   sample rate,
      - phase: initial phase offset (in degrees).
*/
lfoparams* init_lfo(lfoparams* lp, float fosc, float fs, float phase)
{
    // Allocate memory for lfoparams
    lp = (lfoparams*) malloc(sizeof(lfoparams));
    
    float ts = 1.0 / fs;
    float frq = 2.0 * fosc;           // Used in integrated triangle computations
    float t = 4.0 * frq * frq * ts * ts;

    // Phase can be used to introduce a startup delay
    float p = phase / 180.0;  // Convert phase from degrees to fraction
    if(p < 0.0) p = -p;       // No negative phase
    p /= frq;
    p *= fs;
    int phdly = (int) p;
    lp->startup_delay = phdly;

    // Integrated triangle LFO setup (quasi-sinusoidal shape)
    lp->k_ = lp->k = 2.0 * ts * frq;
    lp->nk_ = lp->nk = -2.0 * ts * frq;
    lp->psign_ = lp->psign = t;
    lp->nsign_ = lp->nsign = -t;
    lp->sign_ = lp->sign = t;
    lp->lfo = 0.0;
    lp->x = 0.0;
    
    // Triangle wave LFO variables
    lp->ktri = frq / fs;
    lp->trisign = 1.0;
    p = frq * phase / (360.0 * fosc);
    if(p >= 1.0) {
        p -= 1.0;
        lp->trisign = -1.0;
    }
    if(p < 0.0) {
        p = 0.0;
        lp->trisign = 1.0;
    }
    lp->trilfo = p;
    
    // Sine wave LFO variables
    lp->ksin = M_PI * frq / fs;
    lp->sin_part = sin(2.0 * M_PI * phase / 360.0);
    lp->cos_part = cos(2.0 * M_PI * phase / 360.0);
    
    // RC relaxation oscillator parameters
    float ie = 1.0 / (1.0 - 1.0 / M_E);
    float k = expf(-2.0 * fosc / fs);

    lp->rlx_k = k;
    lp->rlx_ik = 1.0 - k;

    lp->rlx_sign = ie;
    lp->rlx_max = ie;
    lp->rlx_min = 1.0 - ie;
    lp->rlx_lfo = 0.0;

    // Exponential oscillator parameters
    k = expf(-2.0 * 1.3133f * fosc / fs);
    lp->exp_ik = k;
    lp->exp_k = 1.0 / k;
    lp->exp_x = k;
    lp->exp_min = 1.0 / M_E;
    lp->exp_max = 1.0 + 1.0 / M_E;
    lp->exp_sv = lp->exp_min;
    
    // Global fields
    lp->current_rate = fosc;
    lp->lfo_type = 0; // default to integrated triangle

    return lp; 
}

/*
  update_lfo():
    Allows changing the LFO frequency (fosc) at runtime.
    Recomputes constants used by the integrated triangle, triangle, and other shapes.
*/
void update_lfo(lfoparams* lp, float fosc, float fs)
{
    float ts = 1.0 / fs;
    float frq = 2.0 * fosc;
    float t = 4.0 * frq * frq * ts * ts;

    // Keep track of new rate
    lp->current_rate = fosc;
    
    // Integrated triangle LFO re-init
    lp->k_ = 2.0f * ts * frq;
    lp->nk_ = -2.0f * ts * frq;
    lp->psign_ = t;
    lp->nsign_ = -t;
    lp->sign_ = t;
    
    // Triangle wave
    lp->ktri = frq / fs;
    
    // Sine wave
    lp->ksin = M_PI * frq / fs;

    // Relaxation oscillator
    float k = expf(-2.0f * fosc / fs);
    lp->rlx_k = k;
    lp->rlx_ik = 1.0f - k;
    
    // Exponential oscillator
    k = expf(-2.0f * 1.3133f * fosc / fs);
    lp->exp_ik = k;
    lp->exp_k = 1.0f / k;
    
    if(lp->exp_x >= 1.0f)
        lp->exp_x = lp->exp_k;
    else
        lp->exp_x = lp->exp_ik;
        
    lp->exp_min = 1.0f / M_E;
    lp->exp_max = 1.0f + 1.0f / M_E;
    if(lp->exp_sv < lp->exp_min) lp->exp_sv = lp->exp_min;
    if(lp->exp_sv > lp->exp_max) lp->exp_sv = lp->exp_max;
}

/*
  run_integrated_triangle_lfo():
    Produces a quasi-sinusoidal LFO shape by integrating a triangle wave.
    The derivative is purely triangular, leading to a linear pitch shift
    when used to modulate a delay or similar parameter. A startup delay
    (startup_delay) can keep the LFO at 0 until the specified phase offset
    is reached.
*/
float run_integrated_triangle_lfo(lfoparams* lp)
{
    // Wait out the startup delay
    if(lp->startup_delay > 0) {
        lp->startup_delay -= 1;
        lp->lfo = 0.0;
        return 0.0;
    }
    
    // Move x by sign
    lp->x += lp->sign;
    if(lp->x >= lp->k) {
        lp->sign = lp->nsign_;
        // If rate changed, reset
        lp->x = lp->k_;
        lp->k = lp->k_;
        lp->nk = lp->nk_;
    }
    else if(lp->x <= lp->nk) {
        lp->sign = lp->psign_;
        // If rate changed, reset
        lp->x = lp->nk_;
        lp->k = lp->k_;
        lp->nk = lp->nk_;
    }

    lp->lfo += lp->x;
    if(lp->lfo > 1.0)
        lp->lfo = 1.0;
    if(lp->lfo < 0.0)
        lp->lfo = 0.0;
    
    return lp->lfo;
}

/*
  run_triangle_lfo():
    Generates a simple triangle wave in [0..1].
    The sign flips whenever trilfo hits 0.0 or 1.0,
    producing a repeating up/down cycle.
*/
float run_triangle_lfo(lfoparams* lp)
{
    lp->trilfo += lp->ktri * lp->trisign;
    if(lp->trilfo >= 1.0) {
        lp->trisign = -1.0;
    }
    if(lp->trilfo <= 0.0) {
        lp->trisign = 1.0;
    }
    return lp->trilfo;
}

/*
  run_sine_lfo():
    Sine wave LFO implemented by iterating sin_part and cos_part
    with ksin, producing output roughly in [0..1].
*/
float run_sine_lfo(lfoparams* lp)
{
    lp->sin_part += lp->cos_part * lp->ksin;
    lp->cos_part -= lp->sin_part * lp->ksin;
    return 0.5f * (1.0f + lp->cos_part);
}

/*
  run_rlx_lfo():
    A simple RC relaxation oscillator. The output is roughly a
    triangular charge-discharge shape under a 1st-order filter.
    The variable rlx_sign toggles between (1.0+1/e) and -1/e
    when the filter output crosses thresholds 0.0 or 1.0.
*/
float run_rlx_lfo(lfoparams* lp)
{
    // 1st-order filter
    lp->rlx_lfo = lp->rlx_sign * lp->rlx_ik + lp->rlx_k * lp->rlx_lfo;
    
    // Threshold toggles
    if(lp->rlx_lfo >= 1.0) {
        lp->rlx_sign = lp->rlx_min;
    } 
    else if(lp->rlx_lfo <= 0.0) {
        lp->rlx_sign = lp->rlx_max;
    }
    return lp->rlx_lfo;
}

/*
  run_exp_lfo():
    Creates an exponential up/down shape by toggling exp_x
    between exp_k and exp_ik. The output is offset by exp_min,
    so the final range is roughly [0..(exp_max-exp_min)].
*/
float run_exp_lfo(lfoparams* lp)
{
    lp->exp_sv *= lp->exp_x;
    if(lp->exp_sv >= lp->exp_max) {
        lp->exp_x = lp->exp_ik;
    } else if(lp->exp_sv <= lp->exp_min) {
        lp->exp_x = lp->exp_k;
    }
    return lp->exp_sv - lp->exp_min;
}

/*
  run_lfo():
    Wrapper function that chooses the appropriate LFO shape
    based on lp->lfo_type. Returns a float in [0..1], though
    certain shapes can dip slightly out of that range (like
    the 'square' variant).
*/
float run_lfo(lfoparams* lp)
{
    float lfo_out = 0.0f;
    switch(lp->lfo_type)
    {
        case INT_TRI: // integrated triangle wave
            lfo_out = run_integrated_triangle_lfo(lp);
            break;
            
        case TRI: // standard triangle wave
            lfo_out = run_triangle_lfo(lp);
            break;
        
        case SINE: // sine wave
            lfo_out = run_sine_lfo(lp);
            break;
        
        case SQUARE: // click-less square (compressed sine)
            lfo_out = run_sine_lfo(lp) - 0.5f;
            // Amplify and soft-clip the sine wave
            if(lfo_out > 0.0f) {
                lfo_out *= 1.0f / (1.0f + 30.0f * lfo_out);
            } else {
                lfo_out *= 1.0f / (1.0f - 30.0f * lfo_out);
            }
            lfo_out *= 16.0f;
            lfo_out += 0.5f;
            break;
        
        case EXP: // exponential
            lfo_out = run_exp_lfo(lp);
            break;
        
        case RELAX: // RC relaxation oscillator
            lfo_out = run_rlx_lfo(lp);
            break;
            
        case HYPER: // smooth bottom, triangular top
            lfo_out = run_integrated_triangle_lfo(lp);
            lfo_out = 1.0f - fabsf(lfo_out - 0.5f);
            break;
    	
    	case HYPER_SINE:  // sine bottom, triangular top
            lfo_out = run_sine_lfo(lp);
            lfo_out = 1.0f - fabsf(lfo_out - 0.5f);
            break;    	
            
        default:
            // fallback to integrated triangle
            lfo_out = run_integrated_triangle_lfo(lp);
            break;
    }
    return lfo_out;
}

/*
  get_lfo_name():
    Populates outstring with a descriptive name for the LFO type.
    Useful for debugging or user interface prompts.
*/
void get_lfo_name(unsigned int type, char* outstring)
{
	int i;
	for(i=0; i<30; i++)
		outstring[i] = '\0';
		
    switch(type)
    {
        case INT_TRI:
            sprintf(outstring, "INTEGRATED TRIANGLE");
            break;
        case TRI:
            sprintf(outstring, "TRIANGLE");
            break;
        case SINE:
            sprintf(outstring, "SINE");
            break;
        case SQUARE:
            sprintf(outstring, "SQUARE");
            break;
        case EXP:
            sprintf(outstring, "EXPONENTIAL");
            break;
        case RELAX:
            sprintf(outstring, "RC RELAXATION");
            break;
        case HYPER:
            sprintf(outstring, "HYPER");
            break;
        case HYPER_SINE:
            sprintf(outstring, "HYPER_SINE");
            break;
        default:
            sprintf(outstring, "DEFAULT: INTEGRATED TRIANGLE");
            break;
    }
}

/*
  set_lfo_type():
    Allows switching the LFO wave shape at runtime.
*/
void set_lfo_type(lfoparams* lp, unsigned int type) 
{
    lp->lfo_type = type;
}