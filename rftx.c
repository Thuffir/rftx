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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pigpio.h>
#include "gt9000.h"
#include "dmv7008.h"

/***********************************************************************************************************************
 * Init functions
 **********************************************************************************************************************/
static void Init(void)
{
  uint32_t try = 0;

  // Disable interfaces
  gpioCfgInterfaces(PI_DISABLE_FIFO_IF | PI_DISABLE_SOCK_IF);

  // Set sample rate
  if(gpioCfgClock(10, PI_CLOCK_PCM, 0)) {
    perror("gpioCfgClock()");
    exit(EXIT_FAILURE);
  }

  // Initialise GPIO library with retries
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

  // Set Pullups and Pulldowns
  if(gpioSetPullUpDown(OUTPUT_PIN, PI_PUD_OFF)) {
    perror("gpioSetPullUpDown()");
    exit(EXIT_FAILURE);
  }

  // Set GPIO mode
  if(gpioSetMode(OUTPUT_PIN, PI_OUTPUT)) {
    perror("gpioSetMode()");
    exit(EXIT_FAILURE);
  }

  // Set GPIO to Low
  if(gpioWrite(OUTPUT_PIN, 0)) {
    perror("gpioWrite()");
    exit(EXIT_FAILURE);
  }
}

/***********************************************************************************************************************
 * Main
 **********************************************************************************************************************/
int main(int argc, char *argv[])
{
  // Provide help if asked for
  if(argc < 2) {
    printf("Usage:\n");
  }

  // Do init stuff
  Init();

  // Call Module handlers
  Gt9000Handle(argc, argv);
  Dmv7008Handle(argc, argv);

  // Terminate the library and clean up
  gpioTerminate();

  return 0;
}
