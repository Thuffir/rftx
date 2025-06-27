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
#ifdef MODULE_GT9000_ENABLE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pigpio.h>

#include "wave.h"

// Pulse lengths
#define SHORT_PULSE                400
#define LONG_PULSE                1200
#define START_PAUSE               2400

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
 * Add one bit to the waveform (0 -> ‾‾\____, 1 -> ‾‾‾‾\__)
 **********************************************************************************************************************/
static void Gt9000AddBit(BitType bit)
{
  WaveAddPulse(1, bit ? LONG_PULSE : SHORT_PULSE);
  WaveAddPulse(0, bit ? SHORT_PULSE : LONG_PULSE);
}

/***********************************************************************************************************************
 * GT9000 Handler
 **********************************************************************************************************************/
void Gt9000Handle(int argc, char *argv[])
{
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

  // Initialize waveform
  WaveInitialize(
#ifdef DEBUG
  SHORT_PULSE
 #else
   0
 #endif
  );

  // Add Start Pulse
  WaveAddPulse(1, SHORT_PULSE);
  WaveAddPulse(0, START_PAUSE);

  // Add Preamble
  const uint8_t preamble[] = { 1, 1, 0, 0 };
  for(int i = 0; i < sizeof(preamble); i++) {
    Gt9000AddBit(preamble[i]);
  }

  // Add Code
  uint16_t code = Gt9000GetCode(channel, state);
  for(int i = 0; i < (sizeof(code) * 8); i++) {
    Gt9000AddBit((code & (0x8000 >> i)) ? BitOne : BitZero);
  }

  // Add Channel
  uint8_t ch = Gt9000GetChannel(channel);
  for(int i = 0; i < 3; i++) {
    Gt9000AddBit((ch & (0x4 >> i)) ? BitOne : BitZero);
  }

  // Add Trailing Zero Bit
  Gt9000AddBit(BitZero);

  // Transmit wave
  WaveTransmit(NUM_REPEATS);
}

#endif // MODULE_GT9000_ENABLE
