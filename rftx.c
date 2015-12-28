/***********************************************************************************************************************
 *
 *
 *
 * (C) 2015 Gergely Budai
 *
 *
 **********************************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
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

#define NUM_REPEATS                  5

/***********************************************************************************************************************
 * Init functions
 **********************************************************************************************************************/
void Init(void)
{
  // Set smaple rate
  if(gpioCfgClock(10, PI_CLOCK_PCM, 0)) {
    perror("gpioCfgClock()");
    exit(EXIT_FAILURE);
  }

  // Initialise GPIO library
  if(gpioInitialise() < 0) {
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


/***********************************************************************************************************************
 * Main
 **********************************************************************************************************************/
int main(void)
{
  //                   1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
  const char msg[] = { 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0 };
  gpioPulse_t wave[WAVE_SIZE];
  int wave_id, msgidx, waveidx = 0;

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

  for(msgidx = 0; msgidx < MSG_BITS; msgidx++) {
    int firstpulse, secondpulse;

    // One
    if(msg[msgidx]) {
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
  }

  // Do init stuff
  Init();

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

  if(gpioWaveChain((char []) {
    255, 0,
      wave_id,
    255, 1, NUM_REPEATS, 0
  }, 7) < 0) {
    perror("gpioWaveChain()");
    exit(EXIT_FAILURE);
  }
  while(gpioWaveTxBusy());

  gpioTerminate();

  return 0;
}
