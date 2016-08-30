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
#include "ddcci.h"
#include "attacks.h"

#include "shell.h"
#include "chprintf.h"

/*
 * DP resistor control is not possible on the STM32F3-Discovery, using stubs
 * for the connection macros.
 */
#define usb_lld_connect_bus(usbp)
#define usb_lld_disconnect_bus(usbp)

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)

#define MASTER_EDID_REQUEST 0xA1
#define MASTER_WRITE_REQUEST 0xA0
#define MASTER_DDCCI_REQUEST 0x6E
#define MASTER_DDCCI_ANSWER_REQUEST 0x6F
#define MASTER_DDCCI_SOURCE_ADDRESS 0x51
#define MASTER_DDCCI_CAPABILITY_REQUEST 0xF3
#define MASTER_DDCCI_VCP_REQUEST 0x01
#define MASTER_SET_CTRL_ADDRESS 0x03
#define MASTER_SAVE_SETTINGS 0x0C

DEBUG_DEF

uint8_t capAnswer[38];
uint8_t capRequest[6] = {0x6E, 0x51, 0x83, 0xF3, 0x00, 0x00};
uint8_t dummyEDID[128] = /* Dummy EDID with wrong checksum */
{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
uint8_t dummyCap[6] =
{0x6E, 0x83, 0xE3, 0x00, 0x00, 0x00};


uint8_t * ddcRequest; /* cached EDID */
uint8_t * savedEDID;


void Drive_SDA (BBI2C_t *dev, int sda);
void BBI2C_Ack (BBI2C_t *dev);
int atoi (const char *string);
void Drive_SCL (BBI2C_t *dev, int scl);


static void cmd_proxy (BaseSequentialStream *chp, int argc, char *argv[])
{
//  DEBUG_INIT (chp);
  BBI2C_t i2cdev01; /* Slave Mode for PC */
  uint8_t data; /* captured Byte by Proxy */
  uint8_t firstTime = 1;
  uint8_t firstCI = 1;
  uint8_t ddcci_request_length;
  uint8_t retrycap;

  //Slave Device for Host - doe sn't need Start afterwards
  BBI2C_Init (&i2cdev01, GPIOC, 10, GPIOC, 11, 50000, BBI2C_MODE_SLAVE);

  for(;;) /* No STOP - need to listen continuously */
  {
    data = BBI2C_Get_Byte (&i2cdev01);
    switch (data) /* Actions depending on captured byte */
    {
      case MASTER_EDID_REQUEST:

        if(firstTime) /* to have enough time to get edid, send invalid edid */
        {
          write_edid (i2cdev01, dummyEDID);
          firstTime--;
          savedEDID = read_edid (); /* cache valid edid */
          savedEDID = edid_monitor_string_faker (savedEDID);
        }

        if(write_edid (i2cdev01, savedEDID) != 0)
        {
          chprintf(chp, "Writing EDID to Host failed\r\n");
        }
        else /* EDID successfully sent to host */
        {
          chprintf(chp, "Sent EDID to Host\r\n");
        }
        break;

      case MASTER_WRITE_REQUEST:
        break;

      case MASTER_DDCCI_REQUEST:
        data = BBI2C_Get_Byte (&i2cdev01);
        if (data != MASTER_DDCCI_SOURCE_ADDRESS) break; /* break at wrong byte */
        ddcci_request_length = BBI2C_Get_Byte (&i2cdev01); /* read length of the request */
        ddcRequest = ddcci_read_master (&i2cdev01, ddcci_request_length); /* Read request from master */
        data = BBI2C_Get_Byte (&i2cdev01);
        switch (ddcRequest[3])
        {
          case MASTER_DDCCI_CAPABILITY_REQUEST:

            if(ddcRequest[1] == 0xFF) /* invalid request */
            {
              chprintf (chp, "got invalid data for ddc/ci\r\n");
              break;
            }

            if(firstCI)
            {
              if(data == MASTER_DDCCI_ANSWER_REQUEST)
              {
                signed int returncode;
                returncode = ddcci_write_master (dummyCap, 6, 1);
                if (returncode < 0) chprintf(chp, "no ack on bytes\r\n");
                else if (returncode > 0) chprintf(chp, "ack on checksum\r\n");
                else chprintf(chp, "transmission complete\r\n");
              }
              firstCI = 0;

              /* First, gather information from slave about ddc/ci capabilities */
              capRequest[4] = ddcRequest [4];
              capRequest[5] = ddcRequest [5];
              uint8_t retrycap = 5;
              if(ddcci_write_slave (capRequest, 6) < 0)
              {
                  chprintf(chp, "ddcciwrite failed\r\n");
                  break;
              }
              else
              {
                chprintf(chp, "ddcciwrite succeeded\r\n");
                if(ddcci_read_slave(capAnswer) < 0)
                {
                  chprintf(chp, "failed reading ddc/ci, retrying\r\n");
                  while(retrycap)
                  {
                    if(ddcci_write_slave (capRequest, 6) == 0)
                    {
                      if(ddcci_read_slave(capAnswer) < 0)
                      {
                        chprintf(chp, "failed reading ddc/ci, retrying\r\n");
                      }
                      else break;
                    }
                    retrycap--;
                  }
                }
              }
            }
            /* Now, send this information to the host */
            uint8_t messageLength = capAnswer[1] & 0x7F;
            if(data == MASTER_DDCCI_ANSWER_REQUEST)
            {
              chprintf(chp, "Ich sollte jetzt schreiben\r\n");
              signed int returncode;
              returncode = ddcci_write_master (capAnswer, messageLength + 3, 0);
              if (returncode < 0) chprintf(chp, "no ack on bytes\r\n");
              else if (returncode > 0)
              {
                chprintf(chp, "ack on checksum\r\n");
                firstCI = 1;
              }
              else
              {
                chprintf(chp, "transmission complete\r\n");
                firstCI = 1;
              }
            }
            break;
/*
          case MASTER_DDCCI_VCP_REQUEST:
            chprintf(chp, "chp request");

            if(ddcRequest[1] == 0xFF)
            {
              chprintf (chp, "got invalid data for ddc/ci\r\n");
              break;
            }
            retrycap = 5;
            if(ddcci_write_slave (ddcRequest, 5) < 0)
            {
                chprintf(chp, "ddcciwrite failed\r\n");
                break;
            }
            else
            {
              chprintf(chp, "ddcciwrite succeeded\r\n");
              if(ddcci_read_slave(capAnswer) < 0)
              {
                chprintf(chp, "failed reading ddc/ci, retrying\r\n");
                while(retrycap)
                {
                  if(ddcci_write_slave (ddcRequest, 5) == 0)
                  {
                    if(ddcci_read_slave(capAnswer) < 0)
                    {
                      chprintf(chp, "failed reading ddc/ci, retrying\r\n");
                    }
                    else break;
                  }
                  retrycap--;
                }
              }
            }
            messageLength = capAnswer[1] & 0x7F;
            if(data == MASTER_DDCCI_ANSWER_REQUEST)
            {
              chprintf(chp, "Ich sollte jetzt schreiben\r\n");
              returncode = ddcci_write_master (capAnswer, messageLength + 3, 0);
              if (returncode < 0) chprintf(chp, "no ack on bytes\r\n");
              else if (returncode > 0)
              {
                chprintf(chp, "ack on checksum\r\n");
              }
              else
              {
                chprintf(chp, "transmission complete\r\n");
              }
            }
            break;
*/
/*
          case MASTER_SET_CTRL_ADDRESS:
            if(ddcRequest[1] == 0xFF)
            {
              chprintf (chp, "got invalid data for ddc/ci\r\n");
              break;
            }
            retrycap = 5;
            if(ddcci_write_slave (ddcRequest, 7) < 0)
            {
                chprintf(chp, "ddcciwrite failed\r\n");
                break;
            }
            else
            {
              chprintf(chp, "ddcciwrite succeeded\r\n");
              if(ddcci_read_slave(capAnswer) < 0)
              {
                chprintf(chp, "failed reading ddc/ci, retrying\r\n");
                while(retrycap)
                {
                  if(ddcci_write_slave (ddcRequest, 7) == 0)
                  {
                    if(ddcci_read_slave(capAnswer) < 0)
                    {
                      chprintf(chp, "failed reading ddc/ci, retrying\r\n");
                    }
                    else break;
                  }
                  retrycap--;
                }
              }
            }
            break;
*/
/*
          case MASTER_SAVE_SETTINGS:
            if(ddcRequest[1] == 0xFF)
            {
              chprintf (chp, "got invalid data for ddc/ci\r\n");
              break;
            }
            retrycap = 5;
            if(ddcci_write_slave (ddcRequest, 4) < 0)
            {
                chprintf(chp, "ddcciwrite failed\r\n");
                break;
            }
            else
            {
              chprintf(chp, "ddcciwrite succeeded\r\n");
              if(ddcci_read_slave(capAnswer) < 0)
              {
                chprintf(chp, "failed reading ddc/ci, retrying\r\n");
                while(retrycap)
                {
                  if(ddcci_write_slave (ddcRequest, 4) == 0)
                  {
                    if(ddcci_read_slave(capAnswer) < 0)
                    {
                      chprintf(chp, "failed reading ddc/ci, retrying\r\n");
                    }
                    else break;
                  }
                  retrycap--;
                }
              }
            }
            break;
*/
          default:
            break;
        }
        break;

      default:
        break;
    }
  }
}

static void cmd_sample (BaseSequentialStream *chp, int argc, char *argv[])
{
    uint8_t data;
    BBI2C_t i2cdev;
    uint8_t samples[15];
    uint8_t samplecount = 0;

    DEBUG_INIT (chp);

    BBI2C_Init (&i2cdev, GPIOC, 10, GPIOC, 11, 50000, BBI2C_MODE_SLAVE);

    //Store all captured Bytes in an array. Print after 10 captured bytes.
    chprintf (chp, "Sampling Line: ");
    for (;;)
    {
        data = BBI2C_Get_Byte (&i2cdev);
      	if(samplecount < 15)
      	{
          samples[samplecount] = data;
      		samplecount++;
      	}
      	else
      	{
      		for(samplecount = 0; samplecount < 15; samplecount++)
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

static void cmd_edid (BaseSequentialStream *chp, int argc, char *argv[])
{
  uint8_t i;
  uint8_t retry = 3;

  chprintf(chp, "Read EDID: \r\n");
  for(i = 0; i < retry; i++)
  {
    savedEDID = read_edid();
    if(savedEDID[0] == 0xFF)
    {
      chprintf(chp, "Reading EDID failed");
    }
    else
    {
      for(i = 0; i < 128; i++)
      {
        chprintf(chp, "%x ", savedEDID[i]);
      }
      return;
    }
  }
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

static void cmd_ddcci (BaseSequentialStream *chp, int argc, char *argv[])
{

  uint8_t i;
  uint8_t retry = 3;
  uint8_t offhi, offlo;
  uint8_t retrycap;

  chprintf(chp, "Read EDID: \r\n");
  for(i = 0; i < retry; i++)
  {
    savedEDID = read_edid();
    if(savedEDID[0] == 0xFF)
    {
      chprintf(chp, "Reading EDID failed \r\n");
    }
    else
    {
      for(i = 0; i < 128; i++)
      {
        chprintf(chp, "%x ", savedEDID[i]);
      }
      chprintf(chp, "\r\n");
      break;
    }
  }

  chprintf(chp, "Write to DDC/CI\r\n");
  uint16_t offsetlist[10] = {0x00, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xE0, 0xFE, 0x108};
  for(int z = 0; z < 10; z++)
  {
    offhi = offsetlist[z] >> 8;
    offlo = offsetlist[z] & 255;
    capRequest[4] = offhi;
    capRequest[5] = offlo;
    retrycap = 5;

    if(ddcci_write_slave (capRequest, 6) < 0)
    {
      chprintf(chp, "ddcciwrtie failed\r\n");
    }
    else
    {
       chprintf(chp, "ddcciwrite succeeded\r\n");
       if(ddcci_read_slave(capAnswer) < 0)
       {
         chprintf(chp, "failed reading ddc/ci, retrying\r\n");
         while(retrycap)
         {
           if(ddcci_write_slave (capRequest, 6) == 0)
           {
             if(ddcci_read_slave(capAnswer) < 0)
             {
               chprintf(chp, "failed reading ddc/ci, retrying\r\n");
             }  else break;
           }
           retrycap--;
         }
       }
       chThdSleepMilliseconds (50);
    }
    for(i = 0; i < 38; i++)
    {
      chprintf(chp, "%02x ", capAnswer[i]);
    }
    chprintf(chp, "\r\n", capAnswer[i]);
  }

}

static const ShellCommand commands[] = {
  {"proxy", cmd_proxy},
  {"sample", cmd_sample},
  {"pintest", cmd_pintest},
  {"edid", cmd_edid},
  {"ddc", cmd_ddc},
  {"comm", cmd_ddcci},
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
