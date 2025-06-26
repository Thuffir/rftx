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
#ifdef MODULE_DMV7008_ENABLE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "wave.h"

// Pulse lengths
#define SHORT_PULSE                630
#define LONG_PULSE                1292

// Pause between repeats
#define TLG_PAUSE                73700

// Number of telegram repeats
#define NUM_REPEATS                  4

// Upper limit of house code
#define MAX_CODE                 0xFFF

// Name of the module
static const char moduleName[] = "dmv7008";

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
 * Get the Physical channel number
 **********************************************************************************************************************/
static uint8_t Dmv7008GetChannel(ChannelType channel)
{
  // Logical to Physical channel assignment
  static const uint8_t channelTable[] = { 0, 4, 2, 6, 7 };

  return channelTable[channel];
}

/***********************************************************************************************************************
 * Add one bit to the waveform (0 -> __/‾‾‾‾, 1 -> ____/‾‾)
 **********************************************************************************************************************/
static void Dmv7008AddBit(BitType bit)
{
  WaveAddPulse(0, bit ? LONG_PULSE : SHORT_PULSE);
  WaveAddPulse(1, bit ? SHORT_PULSE : LONG_PULSE);
}

/***********************************************************************************************************************
 * GT9000 Handler
 **********************************************************************************************************************/
void Dmv7008Handle(int argc, char *argv[])
{
  // Provide help if asked for
  if(argc < 2) {
    printf(" %s %s housecoode[000-FFF] channel[1-5] state[0-1]\n", argv[0], moduleName);
    return;
  }

  // Check if the arguments are meant for us
  if(strcmp(argv[1], moduleName) != 0) {
    return;
  }

  // There should be 4 arguments (plus program name)
  if(argc != 5) {
    fprintf(stderr, "%s: invalid arguments!\n", moduleName);
    exit(EXIT_FAILURE);
  }

  // Convert house code
  uint16_t code = strtol(argv[2], NULL, 16);
  if(code > MAX_CODE) {
    fprintf(stderr, "%s: invalid house code!\n", moduleName);
    exit(EXIT_FAILURE);
  }

  // Convert channel number
  ChannelType channel = atoi(argv[3]) - 1;
  if(channel >= ChInvalid) {
    fprintf(stderr, "%s: invalid channel!\n", moduleName);
    exit(EXIT_FAILURE);
  }

  // Convert switch state
  StateType state = atoi(argv[4]);
  if(state >= StateInvalid) {
    fprintf(stderr, "%s: invalid state!\n", moduleName);
    exit(EXIT_FAILURE);
  }

  // Init wave
  WaveInitialize(0);
  
  // Add Start Pulse
  WaveAddPulse(1, SHORT_PULSE);

  // Add Housecode (12 Bits)
  for(int i = 0; i < 12; i++) {
    Dmv7008AddBit((code & (0x800 >> i)) ? BitOne : BitZero);
  }

  // Add Channel (3 Bits)
  BitType csum[2] = { BitZero, BitZero }, bit;
  uint8_t ch = Dmv7008GetChannel(channel);
  for(int i = 0; i < 3; i++) {
    bit = (ch & (0x4 >> i)) ? BitOne : BitZero;
    Dmv7008AddBit(bit);
    csum[i % 2] ^= bit;
  }

  // Add Switch state (1 Bit)
  bit = (state) ? BitOne : BitZero;
  Dmv7008AddBit(bit);
  csum[1] ^= bit;

  // Add Dim state (1 Bit) (Todo: Not yet supported.)
  Dmv7008AddBit(BitZero);
  // Don't forget the checksum here

  // Add unknown bit (1 Bit) (Zero)
  Dmv7008AddBit(BitZero);

  // Add Checksum
  Dmv7008AddBit(csum[0]);
  Dmv7008AddBit(csum[1]);

  // Add pause at the end
  WaveAddPulse(0, TLG_PAUSE);
  
  // Transmit wave
  WaveTransmit(NUM_REPEATS);
}

#endif // MODULE_DMV7008_ENABLE
