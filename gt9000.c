/***********************************************************************************************************************
 *
 * Wireless Signal Transmitter for Raspberry Pi
 *
 * (C) 2015 Gergely Budai
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
#ifdef MODULE_GT9000_ENABLE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pigpio.h>

// Message and wave length
#define MSG_BITS                    24
#define WAVE_SIZE                   ((1 + MSG_BITS) * 2)

// Pulse lengths
#define SHORT_PULSE                400
#define LONG_PULSE                1100
#define START_PAUSE               2300

// Alternative start pulse length
#define ALT_START_PULSE           3000
#define ALT_START_PAUSE           7200

// Number of telegram repeats
#define NUM_REPEATS                  8

// Name of the module
static const char moduleName[] = "gt9000";

// Possible states
typedef enum {
  StateOff = 0,
  StateOn  = 1,
  StateInvalid
} StateType;

// Possible Channels
typedef enum {
  Ch1   = 0,
  Ch2   = 1,
  Ch3   = 2,
  Ch4   = 3,
  ChAll = 4,
  ChInvalid
} ChannelType;

// Bit Type
typedef enum {
  BitZero = 0,
  BitOne  = 1,
  BitInvalid
} BitType;

/***********************************************************************************************************************
 * Get a Code corresponding to channel and state
 **********************************************************************************************************************/
static uint16_t Gt9000GetCode(ChannelType channel, StateType state)
{
  // Code groups
  static const uint16_t groupA[] = { 0x8F24, 0xC357, 0x57DB, 0xE5C3 };
  static const uint16_t groupB[] = { 0xBABA, 0x1842, 0x6D01, 0x42F9 };
  // Channel and State to Code group assignment table
  static const uint16_t *codeTable[2][5] = {
    //              Ch1     Ch2      Ch3    Ch4     All
    [StateOff] = { groupB, groupB, groupB, groupA, groupA },
    [StateOn]  = { groupA, groupA, groupA, groupB, groupB }
  };
  static const uint16_t *group;
  uint8_t pick;

  // Get code group
  group = codeTable[state][channel];
  // Pick a time dependent code from code group
  pick = gpioTick() % (sizeof(groupA) / sizeof(groupA[0]));

  return group[pick];
}

/***********************************************************************************************************************
 * Get the Physical channel number
 **********************************************************************************************************************/
static uint8_t Gt9000GetChannel(ChannelType channel)
{
  // Logical to Physical channel assignment
  static const uint8_t channelTable[] = { 0, 2, 6, 1, 5 };

  return channelTable[channel];
}

/***********************************************************************************************************************
 * Add one bit to the waveform
 **********************************************************************************************************************/
static uint32_t Gt9000AddBit(gpioPulse_t *wave, uint32_t waveidx, BitType bit)
{
  uint32_t firstpulse, secondpulse;

  // Bit One
  if(bit) {
    firstpulse = LONG_PULSE;
    secondpulse = SHORT_PULSE;
  }
  // Bit Zero
  else {
    firstpulse = SHORT_PULSE;
    secondpulse = LONG_PULSE;
  }

  // Add first pulse (High)
  wave[waveidx].gpioOn = (1 << OUTPUT_PIN);
  wave[waveidx].gpioOff = 0;
  wave[waveidx].usDelay = firstpulse;
  waveidx++;

  // Add second pulse (Low)
  wave[waveidx].gpioOn = 0;
  wave[waveidx].gpioOff = (1 << OUTPUT_PIN);
  wave[waveidx].usDelay = secondpulse;
  waveidx++;

  return waveidx;
}

/***********************************************************************************************************************
 * Build Waveform to transmit
 **********************************************************************************************************************/
static int Gt9000BuildWave(ChannelType channel, StateType state)
{
  // Premable bits
  const uint8_t preamble[] = { 1, 1, 0, 0 };
  // The waveform
  gpioPulse_t wave[WAVE_SIZE];
  int wave_id;
  uint32_t i, waveidx = 0;
  uint16_t code;
  uint8_t ch;

  // Add Start Pulse
  wave[waveidx].gpioOn = (1 << OUTPUT_PIN);
  wave[waveidx].gpioOff = 0;
  wave[waveidx].usDelay = SHORT_PULSE;
//  wave[waveidx].usDelay = ALT_START_PULSE;
  waveidx++;
  wave[waveidx].gpioOn = 0;
  wave[waveidx].gpioOff = (1 << OUTPUT_PIN);
  wave[waveidx].usDelay = START_PAUSE;
//  wave[waveidx].usDelay = ALT_START_PAUSE;
  waveidx++;

  // Add Preamble
  for(i = 0; i < sizeof(preamble); i++) {
    waveidx = Gt9000AddBit(wave, waveidx, preamble[i]);
  }

  // Add Code
  code = Gt9000GetCode(channel, state);
  for(i = 0; i < (sizeof(code) * 8); i++) {
    waveidx = Gt9000AddBit(wave, waveidx, (code & (0x8000 >> i)) ? BitOne : BitZero);
  }

  // Add Channel
  ch = Gt9000GetChannel(channel);
  for(i = 0; i < 3; i++) {
    waveidx = Gt9000AddBit(wave, waveidx, (ch & (0x4 >> i)) ? BitOne : BitZero);
  }

  // Add Trailing Zero Bit
  waveidx = Gt9000AddBit(wave, waveidx, BitZero);

  // Add a new empty waveform
  if(gpioWaveAddNew()) {
    perror("gpioWaveAddNew()");
    exit(EXIT_FAILURE);
  }
  // Add pulses to the waveform
  if(gpioWaveAddGeneric(WAVE_SIZE, wave) < 0) {
    perror("gpioWaveAddGeneric()");
    exit(EXIT_FAILURE);
  }
  // Create waveform
  if((wave_id = gpioWaveCreate()) < 0) {
    perror("gpioWaveCreate()");
    exit(EXIT_FAILURE);
  }

  return wave_id;
}

/***********************************************************************************************************************
 * GT9000 Handler
 **********************************************************************************************************************/
void Gt9000Handle(int argc, char *argv[])
{
  int wave_id;
  ChannelType channel;
  StateType state;

  // Provide help if asked for
  if(argc < 2) {
    printf(" %s %s channel[1-5] state[0-1]\n", argv[0], moduleName);
    return;
  }

  // Check if the arguments are meant for us
  if(strcmp(argv[1], moduleName) != 0) {
    return;
  }

  // There should be 3 arguments (plus program name)
  if(argc != 4) {
    fprintf(stderr, "%s: invalid arguments!\n", moduleName);
    exit(EXIT_FAILURE);
  }

  // Convert channel number
  channel = atoi(argv[2]) - 1;
  if(channel >= ChInvalid) {
    fprintf(stderr, "%s: invalid channel!\n", moduleName);
    exit(EXIT_FAILURE);
  }

  // Convert switch state
  state = atoi(argv[3]);
  if(state >= StateInvalid) {
    fprintf(stderr, "%s: invalid state!\n", moduleName);
    exit(EXIT_FAILURE);
  }

  // Build Telegram Waveform
  wave_id = Gt9000BuildWave(channel, state);

  // Transmit the waveform NUM_REPEATS times
  if(gpioWaveChain((char []) {
    255, 0,
      wave_id,
    255, 1, NUM_REPEATS, 0
  }, 7) < 0) {
    perror("gpioWaveChain()");
    exit(EXIT_FAILURE);
  }

  // Wait until the transmission has been sent out
  while(gpioWaveTxBusy()) {
    time_sleep(0.1);
  }

  //  Delete the wave
  if(gpioWaveDelete(wave_id) < 0) {
    perror("gpioWaveDelete()");
    exit(EXIT_FAILURE);
  }
}

#endif // MODULE_GT9000_ENABLE
