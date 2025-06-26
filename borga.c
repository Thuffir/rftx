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
#ifdef MODULE_BORGA_ENABLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "wave.h"

// Pulse lengths [uS]
#define SHORT_PULSE    400
#define LONG_PULSE     800
// Pause length between repeats [uS]
#define PAUSE_LENGTH 10000
// Number of repeats
#define NUM_REPEATS     10

// Name of the module
static const char moduleName[] = "borga";

/***********************************************************************************************************************
 * Add one bit to the waveform (0 -> ____/‾‾, 1 -> __/‾‾‾‾)
 **********************************************************************************************************************/
static void BorgaAddBit(bool bit)
{
  WaveAddPulse(0, bit ? SHORT_PULSE : LONG_PULSE);
  WaveAddPulse(1, bit ? LONG_PULSE : SHORT_PULSE);
}

/***********************************************************************************************************************
 * Borga Handler
 **********************************************************************************************************************/
void BorgaHandle(int argc, char *argv[])
{
  uint8_t channel;
  char command;

  // Provide help if asked for
  if(argc < 2) {
    printf(" %s %s channel[0-15] [F]an/[L]ight/[S]peed/[T]imer\n", argv[0], moduleName);
    return;
  }

  // Check if the arguments are meant for us
  if(strcmp(argv[1], moduleName) != 0) {
    return;
  }

  // There should be 2 arguments (plus program name and module name)
  if(argc != 4) {
    fprintf(stderr, "%s: invalid arguments!\n", moduleName);
    exit(EXIT_FAILURE);
  }

  // Convert channel number
  channel = atoi(argv[2]);
  if(channel > 15) {
    fprintf(stderr, "%s: invalid channel!\n", moduleName);
    exit(EXIT_FAILURE);
  }

  // Store command char
  command = toupper(argv[3][0]);

  // Reset waveform
  WaveInitialize();

  // Add start pulse
  WaveAddPulse(1, SHORT_PULSE);

  // Bit 0..1: Unknown
  BorgaAddBit(0);
  BorgaAddBit(0);
  // Bit 2: Fan toggle
  BorgaAddBit(command == 'F');
  // Bit 3: Unknown
  BorgaAddBit(0);
  // Bit 4: Unknown (Reverse toggle?)
  BorgaAddBit(0);
  // Bit 5: Timer
  BorgaAddBit(command == 'T');
  // Bit 6: Speed
  BorgaAddBit(command == 'S');
  // Bit 7: Light toggle
  BorgaAddBit(command == 'L');
  // Bit 8..11: Address
  for(uint8_t mask = 8; mask != 0; mask >>= 1) {
    BorgaAddBit(channel & mask);
  }

  // Add pause at the end
  WaveAddPulse(0, PAUSE_LENGTH);

  // Transmit wave
  WaveTransmit(NUM_REPEATS);
}

#endif // MODULE_BORGA_ENABLE
