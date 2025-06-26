/***********************************************************************************************************************
 *
 * Wireless Signal Transmitter for Raspberry Pi
 *
 * By Gergely Budai
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 *
 **********************************************************************************************************************/

#include "config.h"
#include "wave.h"

#include <stdlib.h>
#include <stdio.h>
#include <pigpio.h>

// Short pulse length for debug visualisation (0 -> disable)
static uint32_t waveDebugShortPulseLength = 0;

// Running wave time
static uint32_t waveTime = 0;

/***********************************************************************************************************************
 * Initialize a new wave
 **********************************************************************************************************************/
void WaveInitialize(uint32_t debugShortPulseLength)
{
  // Clear all waves
  if(gpioWaveClear()) {
    perror("gpioWaveClear()");
    exit(EXIT_FAILURE);
  }

  // Reset wave time
  waveTime = 0;

  // Set debug short pulse length
  waveDebugShortPulseLength = debugShortPulseLength;
}

/***********************************************************************************************************************
 * Add one pulse to the waveform
 **********************************************************************************************************************/
void WaveAddPulse(bool level, uint32_t duration)
{
  gpioPulse_t wave[2];

  // We need to start with a delay to be able to append to the wave
  wave[0].gpioOn = 0;
  wave[0].gpioOff = 0;
  wave[0].usDelay = waveTime;

  // Define pulse
  if(level) {
    // High
    wave[1].gpioOn  = 1 << OUTPUT_PIN;
    wave[1].gpioOff = 0;
  }
  else {
    // Low
    wave[1].gpioOn  = 0;
    wave[1].gpioOff = 1 << OUTPUT_PIN;
  }
  wave[1].usDelay = duration;

  // Update end marker
  waveTime += duration;

  // Add pulse to wave
  if(gpioWaveAddGeneric(2, wave) < 0) {
    perror("gpioWaveAddGeneric()");
    exit(EXIT_FAILURE);
  }

  // Show debug
  if(waveDebugShortPulseLength) {
    for(int i = 0; i < (duration / (waveDebugShortPulseLength / 2)); i++) {
      printf(level ? "‾" : "_");
    }
  }
}

/***********************************************************************************************************************
 * Transmit waveform
 **********************************************************************************************************************/
void WaveTransmit(uint32_t repetitions)
{
  int wave_id;

  // Close debug message
  if(waveDebugShortPulseLength) {
    printf(" (%u µS) x %u\n", waveTime, repetitions);
  }

  // Create waveform
  if((wave_id = gpioWaveCreate()) < 0) {
    perror("gpioWaveCreate()");
    exit(EXIT_FAILURE);
  }

  // Transmit the waveform 'repetitions' times
  if(gpioWaveChain((char []) {
    255, 0,
      wave_id,
    255, 1, repetitions, 0
  }, 7) < 0) {
    perror("gpioWaveChain()");
    exit(EXIT_FAILURE);
  }

  // Wait until the transmission has been sent out
  while(gpioWaveTxBusy()) {
    time_sleep(WAVE_TX_POLL_DELAY);
  }

  //  Delete the wave
  if(gpioWaveDelete(wave_id) < 0) {
    perror("gpioWaveDelete()");
    exit(EXIT_FAILURE);
  }
}
