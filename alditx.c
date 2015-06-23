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

#define MSG_BITS                    20
#define WAVE_SIZE                   ((MSG_BITS * 2) + 2)

#define SHORT_PULSE                630
#define LONG_PULSE                1292

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
  //             1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20
//char msg[] = { 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
  char msg[] = { 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  gpioPulse_t wave[WAVE_SIZE];
  int wave_id, msgidx, waveidx = 0;

  wave[waveidx].gpioOn = (1 << OUTPUT_PIN);
  wave[waveidx].gpioOff = 0;
  wave[waveidx].usDelay = SHORT_PULSE;
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

    wave[waveidx].gpioOn = 0;
    wave[waveidx].gpioOff = (1 << OUTPUT_PIN);
    wave[waveidx].usDelay = firstpulse;
    waveidx++;

    wave[waveidx].gpioOn = (1 << OUTPUT_PIN);
    wave[waveidx].gpioOff = 0;
    wave[waveidx].usDelay = secondpulse;
    waveidx++;
  }

  wave[waveidx].gpioOn = 0;
  wave[waveidx].gpioOff = (1 << OUTPUT_PIN);
  wave[waveidx].usDelay = SHORT_PULSE;

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
  if(gpioWaveTxSend(wave_id, PI_WAVE_MODE_ONE_SHOT) < 0) {
    perror("gpioWaveTxSend()");
    exit(EXIT_FAILURE);
  }

  while(gpioWaveTxBusy());
  gpioTerminate();

  return 0;
}
