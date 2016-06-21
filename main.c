/*
    ChibiOS - Copyright (C) 2006..2015 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.h"
#include "hal.h"
#include "usbcfg.h"

#include "shell.h"
#include "chprintf.h"

/*
 * DP resistor control is not possible on the STM32F3-Discovery, using stubs
 * for the connection macros.
 */
#define usb_lld_connect_bus(usbp)
#define usb_lld_disconnect_bus(usbp)

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)

static void cmd_ddc (BaseSequentialStream *chp, int argc, char *argv[])
{

  (void)argv;

    msg_t status;
    static const uint8_t cmd[] = {0xde, 0xad};
    uint8_t data[16];
    static i2cflags_t errors = 0;

    if (argc != 1)
    {
        chprintf(chp, "Invalid arguments (expected 1, got %d)\r\n", argc);
        return;
    }
    uint8_t addr = atoi(argv[0]);

    chprintf(chp, "Sending command to %x\r\n", addr);
    //i2cAcquireBus(&I2CD1);
    status = i2cMasterTransmitTimeout(&I2CD2, addr, cmd, sizeof(cmd), data, sizeof(data), TIME_INFINITE);
    //i2cReleaseBus(&I2CD1);

    switch (status)
    {
        case MSG_RESET:
            errors = i2cGetErrors(&I2CD2);
            chprintf(chp, "Error sending I2C command (%x)\r\n", errors);
            break;
        case MSG_TIMEOUT:
            chprintf(chp, "Timeout sending I2C command.\r\n");
            break;
        case MSG_OK:
            chprintf(chp, "Done sending I2C command\r\n");
            break;
        default:
            chprintf(chp, "??? Invalid i2cMasterTransmitTimeout result code.\r\n");
            break;
    }
}

static const ShellCommand commands[] = {
  {"ddc", cmd_ddc},
  {NULL, NULL}
};


static const ShellConfig shell_cfg1 = {
  (BaseSequentialStream *)&SDU1,
  commands
};

static const I2CConfig i2cconfig = {
    .timingr = 
        STM32_TIMINGR_PRESC  (15U) |
        STM32_TIMINGR_SCLDEL  (4U) |
        STM32_TIMINGR_SDADEL  (2U) |
        STM32_TIMINGR_SCLH   (15U) |
        STM32_TIMINGR_SCLL   (21U),
    .cr1 = 0,
    .cr2 = 0
};

/*
 * Blinker thread #1.
 */
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {

  (void)arg;

  chRegSetThreadName("blinker");
  while (true) {
    palSetPad(GPIOE, GPIOE_LED3_RED);
    chThdSleepMilliseconds(125);
    palClearPad(GPIOE, GPIOE_LED3_RED);
    chThdSleepMilliseconds(125);
    palSetPad(GPIOE, GPIOE_LED7_GREEN);
    chThdSleepMilliseconds(125);
    palClearPad(GPIOE, GPIOE_LED7_GREEN);
    chThdSleepMilliseconds(125);
    palSetPad(GPIOE, GPIOE_LED10_RED);
    chThdSleepMilliseconds(125);
    palClearPad(GPIOE, GPIOE_LED10_RED);
    chThdSleepMilliseconds(125);
    palSetPad(GPIOE, GPIOE_LED6_GREEN);
    chThdSleepMilliseconds(125);
    palClearPad(GPIOE, GPIOE_LED6_GREEN);
    chThdSleepMilliseconds(125);
  }
}

/*
 * Application entry point.
 */
int main(void) {

  thread_t *shelltp = NULL;

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * Initializes a serial-over-USB CDC driver.
   */
  sduObjectInit(&SDU1);
  sduStart(&SDU1, &serusbcfg);

  /*
   * Activates the USB driver and then the USB bus pull-up on D+.
   * Note, a delay is inserted in order to not have to disconnect the cable
   * after a reset.
   */
  usbDisconnectBus(serusbcfg.usbp);
  chThdSleepMilliseconds(1500);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);

  /*
   * Shell manager initialization.
   */
  shellInit();

  /*
   * Initialize I2C
   */
  palSetPadMode(GPIOA, 14, PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN);   /* I2C1 SDA */
  palSetPadMode(GPIOA, 15, PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN);   /* I2C1 SCL */
  palSetPadMode(GPIOA,  9, PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN);   /* I2C2 SCL */
  palSetPadMode(GPIOA, 10, PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN);   /* I2C2 SDA */
  i2cStart(&I2CD2, &i2cconfig);

  /*
   * Creates the example threads.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO+1, Thread1, NULL);

  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop 
   */
  while (true) {

    if (!shelltp && (SDU1.config->usbp->state == USB_ACTIVE))
      shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
    else if (chThdTerminatedX(shelltp)) {
      chThdRelease(shelltp);    /* Recovers memory of the previous shell.   */
      shelltp = NULL;           /* Triggers spawning of a new shell.        */
    }
    chThdSleepMilliseconds(200);
  }
}
