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
#include "bbi2c.h"
#include "debug.h"

#include "shell.h"
#include "chprintf.h"

/*
 * DP resistor control is not possible on the STM32F3-Discovery, using stubs
 * for the connection macros.
 */
#define usb_lld_connect_bus(usbp)
#define usb_lld_disconnect_bus(usbp)

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)

DEBUG_DEF

static void cmd_sample (BaseSequentialStream *chp, int argc, char *argv[])
{
    uint8_t data;
    BBI2C_t i2cdev;
    uint8_t samples[30];
    uint8_t samplecount = 0;

    DEBUG_INIT (chp);

    BBI2C_Init (&i2cdev, GPIOC, 10, GPIOC, 11, 50000, BBI2C_MODE_SLAVE);

    //Store all captured Bytes in an array. Print after 10 captured bytes.
    chprintf (chp, "Sampling Line: ");
    for (;;)
    { 
        data = BBI2C_Get_Byte (&i2cdev);
	if(samplecount < 30)
	{
        samples[samplecount] = data;
		samplecount++;
	}
	else
	{
		for(samplecount = 0; samplecount < 30; samplecount++)
		{
			chprintf(chp, "%x ", samples[samplecount]);
		}
		samplecount = 0;
	}
    }
}

static void cmd_pintest (BaseSequentialStream *chp, int argc, char *argv[])
{
    BBI2C_t i2cdev;

    BBI2C_Init (&i2cdev, GPIOC, 10, GPIOC, 11, 50000, BBI2C_MODE_SLAVE);
    for (;;)
    {
        chThdSleepMilliseconds(100);
        Drive_SDA (&i2cdev, 0);
        chThdSleepMilliseconds(100);
        Drive_SDA (&i2cdev, 1);
    }
}

static void cmd_basicread (BaseSequentialStream *chp, int argc, char *argv[])
{
	BBI2C_t i2cdev;
	int i, k, ack;
	uint8_t stream[8];
	int addr = 0xA1;

	BBI2C_Init (&i2cdev, GPIOC, 10, GPIOC, 11, 50000, BBI2C_MODE_MASTER);
	BBI2C_Start (&i2cdev);

        ack = BBI2C_Send_Byte (&i2cdev, addr);
	if(ack)
	{
		chprintf(chp, "Read EDID: \r\n");
		for(k = 0; k < 16; k++)
		{	
			for(i = 0; i < 8; i++)
			{
				BBI2C_Recv_Byte (&i2cdev, &stream[i]);
				if (k==15 && i==7)
				{
					BBI2C_NACK (&i2cdev);	
				}
				else BBI2C_Ack (&i2cdev);
			}
			chprintf (chp, " %x %x %x %x %x %x %x %x \r\n", stream[0], stream[1], stream[2], stream[3], stream[4], stream[5], stream[6], stream[7]);
		}
	}
	else
	{
		chprintf(chp, "NACK on request - try again!\r\n");
		BBI2C_NACK(&i2cdev);
		BBI2C_Stop(&i2cdev);
		return;
	}
       BBI2C_Stop(&i2cdev);
}

static void cmd_ddc (BaseSequentialStream *chp, int argc, char *argv[])
{
    BBI2C_t i2cdev;
    int i, ack, addr;
    uint8_t header[8];

    if (argc != 1)
    {
        chprintf (chp, "Argument error.\r\n");
        return;
    }

    addr = atoi (argv[0]);
    chprintf (chp, "Sending to %x\r\n", addr);

    BBI2C_Init (&i2cdev, GPIOC, 10, GPIOC, 11, 50000, BBI2C_MODE_MASTER);

    BBI2C_Start (&i2cdev);
    ack = BBI2C_Send_Byte (&i2cdev, addr);
    if (!ack)
    {
        BBI2C_Stop (&i2cdev);
        chprintf (chp, "Error - got no ack in response\r\n");
        return;
    }

    // Read the first 8 bytes (should be fixed header pattern 00 FF FF FF FF FF FF 00
    for (i = 0; i < 8; i++)
    {
        BBI2C_Recv_Byte (&i2cdev, &header[i]);
	BBI2C_Ack (&i2cdev);
        if (!ack)
        {
            BBI2C_Stop (&i2cdev);
            chprintf (chp, "Error - got no ack when reading \r\n");
            return;
        }
    }
   BBI2C_NACK (&i2cdev);
   BBI2C_Stop (&i2cdev);
   chprintf (chp, "Sent command to %x, ack: %d, result: %2x%2x%2x%2x%2x%2x%2x%2x: %d\r\n", addr, ack, header[0], header[1], header[2], header[3], header[4], header[5], header[6], header[7]);
}

static const ShellCommand commands[] = {
  {"ddc", cmd_ddc},
  {"sample", cmd_sample},
  {"basicread", cmd_basicread},
  {"pintest", cmd_pintest},
  {NULL, NULL}
};


static const ShellConfig shell_cfg1 = {
  (BaseSequentialStream *)&SDU1,
  commands
};

/*
 * Blinker thread #1.
 */
static THD_WORKING_AREA(blinkerThreadWA, 128);
static THD_FUNCTION(blinkerThread, arg)
{
    (void)arg;

    chRegSetThreadName("blinker");
    while (true)
    {
        palSetPad(GPIOE, GPIOE_LED3_RED);
        chThdSleepMilliseconds(30);
        palClearPad(GPIOE, GPIOE_LED3_RED);
        chThdSleepMilliseconds(1000);
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
   * MCO
   */
  palSetPadMode(GPIOA, 8, PAL_MODE_ALTERNATE(0));
  

  /*
   * Creates the example threads.
   */
  chThdCreateStatic(blinkerThreadWA, sizeof(blinkerThreadWA), NORMALPRIO+10, blinkerThread, NULL);

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
