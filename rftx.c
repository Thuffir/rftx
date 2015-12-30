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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pigpio.h>

// GPIO PIN
#define OUTPUT_PIN                  24

#define MSG_BITS                    24
#define WAVE_SIZE                   ((1 + MSG_BITS) * 2)

#define SHORT_PULSE                400
#define LONG_PULSE                1100
#define START_PAUSE               2300

#define ALT_START_PULSE           3000
#define ALT_START_PAUSE           7200

#define NUM_REPEATS                  8

#define INIT_TRIES                 100
#define INIT_TRY_SLEEP             0.1

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
 * Init functions
 **********************************************************************************************************************/
static void Init(void)
{
  uint32_t try = 0;

  // Set sample rate
  if(gpioCfgClock(10, PI_CLOCK_PCM, 0)) {
    perror("gpioCfgClock()");
    exit(EXIT_FAILURE);
  }

  // Initialise GPIO library
  for(try = 0; try < INIT_TRIES; try++) {
    if(gpioInitialise() >= 0) {
      break;
    }
    time_sleep(INIT_TRY_SLEEP);
  }
  if(try >= INIT_TRIES) {
    perror("gpioInitialise()");
    exit(EXIT_FAILURE);
  }

  if(gpioSetPullUpDown(OUTPUT_PIN, PI_PUD_OFF)) {
    perror("gpioSetPullUpDown()");
    exit(EXIT_FAILURE);
  }

  if(gpioSetMode(OUTPUT_PIN, PI_OUTPUT)) {
    perror("gpioSetMode()");
    exit(EXIT_FAILURE);
  }

  if(gpioWrite(OUTPUT_PIN, 0)) {
    perror("gpioWrite()");
    exit(EXIT_FAILURE);
  }
}

static uint16_t GetCode(ChannelType channel, StateType state)
{
  static const uint16_t groupA[] = { 0x8F24, 0xC357, 0x57DB, 0xE5C3 };
  static const uint16_t groupB[] = { 0xBABA, 0x1842, 0x6D01, 0x42F9 };
  static const uint16_t *codeTable[2][5] = {
    //              Ch1     Ch2      Ch3    Ch4     All
    [StateOff] = { groupB, groupB, groupB, groupA, groupA },
    [StateOn]  = { groupA, groupA, groupA, groupB, groupB }
  };
  static const uint16_t *group;
  uint8_t pick;

  group = codeTable[state][channel];
  pick = gpioTick() % (sizeof(groupA) / sizeof(groupA[0]));

  return group[pick];
}

static uint8_t GetChannel(ChannelType channel)
{
  static const uint8_t channelTable[] = { 0, 2, 6, 1, 5 };

  return channelTable[channel];
}

static uint32_t AddBit(gpioPulse_t *wave, uint32_t waveidx, BitType bit)
{
  uint32_t firstpulse, secondpulse;

  // One
  if(bit) {
    firstpulse = LONG_PULSE;
    secondpulse = SHORT_PULSE;
  }
  // Zero
  else {
    firstpulse = SHORT_PULSE;
    secondpulse = LONG_PULSE;
  }

  wave[waveidx].gpioOn = (1 << OUTPUT_PIN);
  wave[waveidx].gpioOff = 0;
  wave[waveidx].usDelay = firstpulse;
  waveidx++;

  wave[waveidx].gpioOn = 0;
  wave[waveidx].gpioOff = (1 << OUTPUT_PIN);
  wave[waveidx].usDelay = secondpulse;
  waveidx++;

  return waveidx;
}

static int BuildWave(ChannelType channel, StateType state)
{

  const uint8_t preamble[] = { 1, 1, 0, 0 };
  gpioPulse_t wave[WAVE_SIZE];
  int wave_id;
  uint32_t i, waveidx = 0;
  uint16_t code;
  uint8_t ch;

  // Start Pulse
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

  // Preamble
  for(i = 0; i < sizeof(preamble); i++) {
    waveidx = AddBit(wave, waveidx, preamble[i]);
  }

  // Code
  code = GetCode(channel, state);
  for(i = 0; i < (sizeof(code) * 8); i++) {
    waveidx = AddBit(wave, waveidx, (code & (0x8000 >> i)) ? BitOne : BitZero);
  }

  // Channel
  ch = GetChannel(channel);
  for(i = 0; i < 3; i++) {
    waveidx = AddBit(wave, waveidx, (ch & (0x4 >> i)) ? BitOne : BitZero);
  }

  // Trailing Zero Bit
  waveidx = AddBit(wave, waveidx, BitZero);

  if(gpioWaveAddNew()) {
    perror("gpioWaveAddNew()");
    exit(EXIT_FAILURE);
  }
  if(gpioWaveAddGeneric(WAVE_SIZE, wave) < 0) {
    perror("gpioWaveAddGeneric()");
    exit(EXIT_FAILURE);
  }
  if((wave_id = gpioWaveCreate()) < 0) {
    perror("gpioWaveCreate()");
    exit(EXIT_FAILURE);
  }

  return wave_id;
}

/***********************************************************************************************************************
 * Main
 **********************************************************************************************************************/
int main(int argc, char *argv[])
{
  int wave_id;
  ChannelType channel;
  StateType state;

  if(argc != 3) {
    fprintf(stderr, "arguments\n");
    exit(EXIT_FAILURE);
  }

  channel = atoi(argv[1]) - 1;
  if(channel >= ChInvalid) {
    fprintf(stderr, "channel\n");
    exit(EXIT_FAILURE);
  }

  state = atoi(argv[2]);
  if(state >= StateInvalid) {
    fprintf(stderr, "state\n");
    exit(EXIT_FAILURE);
  }

  // Do init stuff
  Init();

  wave_id = BuildWave(channel, state);

  if(gpioWaveChain((char []) {
    255, 0,
      wave_id,
    255, 1, NUM_REPEATS, 0
  }, 7) < 0) {
    perror("gpioWaveChain()");
    exit(EXIT_FAILURE);
  }
  while(gpioWaveTxBusy()) {
    time_sleep(0.1);
  }

  gpioTerminate();

  return 0;
}
