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

uint8_t savedEDID[128];
uint8_t capAnswer[38];
uint8_t capCopy[38] = {0x6E, 0xA3, 0xE3, 0x00, 0x00, 0x28, 0x70, 0x72, 0x6F,
  0x74, 0x28, 0x6D, 0x6F, 0x6E, 0x69, 0x74, 0x6F, 0x72, 0x29, 0x74, 0x79, 0x70,
  0x65, 0x28, 0x6C, 0x63,
  0x64, 0x29, 0x20, 0x6D, 0x6F, 0x64, 0x65, 0x6C, 0x28, 0x78, 0x78, 0x37};
uint8_t capCopy1[38] = {0x6E, 0xA3, 0xE3, 0x00, 0x20, 0x4D, 0x42, 0x36, 0x35, 0x29, 0x63, 0x6D,
  0x64, 0x73, 0x28, 0x30, 0x31, 0x20, 0x30, 0x32, 0x20, 0x30, 0x33,
  0x20, 0x30, 0x43, 0x20, 0x45, 0x33, 0x20, 0x46, 0x33, 0x29, 0x76, 0x63, 0x70, 0x28, 0x7E};
uint8_t capCopy2[38] = {0x6E, 0xA3, 0xE3, 0x00, 0x40, 0x30, 0x32, 0x30, 0x33, 0x30, 0x34, 0x30, 0x35,
  0x30, 0x36, 0x30, 0x38, 0x30, 0x42, 0x30, 0x43, 0x30, 0x45,
0x31, 0x30, 0x31, 0x32, 0x31, 0x34, 0x28, 0x30, 0x31, 0x20, 0x30, 0x35, 0x20, 0x30, 0x5F};
uint8_t capCopy3[38] = {0x6E, 0xA3, 0xE3, 0x00, 0x60, 0x36, 0x20, 0x30, 0x37, 0x20, 0x30, 0x38,
  0x20, 0x30, 0x42, 0x29, 0x31, 0x36, 0x31, 0x38, 0x31, 0x41, 0x31,
   0x45, 0x32, 0x30, 0x33, 0x30, 0x33, 0x45, 0x35, 0x32, 0x36, 0x30, 0x28, 0x30, 0x31, 0x09};
uint8_t capCopy4[38] = {0x6E, 0xA3, 0xE3, 0x00, 0x80, 0x20, 0x30, 0x33, 0x20, 0x30, 0x34, 0x29, 0x36,
  0x43, 0x36, 0x45, 0x37, 0x30, 0x38, 0x37, 0x41, 0x43, 0x41,
  0x45, 0x42, 0x36, 0x43, 0x30, 0x43, 0x36, 0x43, 0x38, 0x43, 0x39, 0x44, 0x36, 0x28, 0xF1};
uint8_t capCopy5[38] = {0x6E, 0xA3, 0xE3, 0x00, 0xA0, 0x30, 0x31, 0x20, 0x30, 0x34, 0x29, 0x44,
  0x46, 0x45, 0x30, 0x45, 0x31, 0x45, 0x33, 0x28, 0x30, 0x30, 0x20,
  0x30, 0x31, 0x20, 0x30, 0x32, 0x20, 0x30, 0x33, 0x20, 0x30, 0x34, 0x20, 0x31, 0x30, 0xAA};
uint8_t capCopy6[38] = {0x6E, 0xA3, 0xE3, 0x00, 0xC0, 0x20, 0x31, 0x31, 0x20, 0x31, 0x32, 0x20,
  0x31, 0x33, 0x20, 0x31, 0x34, 0x29, 0x45, 0x43, 0x46, 0x44, 0x28,
   0x30, 0x30, 0x20, 0x30, 0x31, 0x29, 0x46, 0x45, 0x28, 0x30, 0x30, 0x20, 0x30, 0x31, 0xBD};
uint8_t capCopy7[36] = {0x6E, 0xA1, 0xE3, 0x00, 0xE0, 0x20, 0x30, 0x32, 0x29, 0x46, 0x46, 0x29, 0x6D,
  0x73, 0x77, 0x68, 0x71, 0x6C, 0x28, 0x31, 0x29,
  0x6D, 0x63, 0x63, 0x73, 0x5F, 0x76, 0x65, 0x72, 0x28, 0x32, 0x2E, 0x31, 0x29, 0x29, 0xB7};
uint8_t capCopy8[6] = {0x6E, 0x83, 0xE3, 0x00, 0xFE, 0xA0};
uint8_t capCopy9[6] = {0x6E, 0x83, 0xE3, 0x01, 0x08, 0x57};

/*uint8_t capCopyLo[38] = {0x6E, 0xA3, 0xE3, 0x00, 0x00, 0x28, 0x70,
  0x72, 0x6F, 0x74, 0x28, 0x6D, 0x6F, 0x6E, 0x69, 0x74, 0x6F, 0x72,
  0x29, 0x74, 0x79, 0x70, 0x65, 0x28, 0x6C, 0x63, 0x64, 0x29, 0x20,
  0x6D, 0x6F, 0x64, 0x65, 0x6C, 0x28, 0x78, 0x78, 0x37};*/
uint8_t capRequest[6] = {0x6E, 0x51, 0x83, 0xF3};
uint8_t * ddcRequest;

void Drive_SDA (BBI2C_t *dev, int sda);
void BBI2C_Ack (BBI2C_t *dev);
int atoi (const char *string);

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

static void cmd_pipe (BaseSequentialStream *chp, int argc, char *argv[])
{
    uint8_t data;
    BBI2C_t i2cdev;
    uint8_t count;
    uint8_t ack;
    uint32_t retry = 10;
    uint32_t ackcounter = 0;
    uint8_t testval = 3;

    DEBUG_INIT (chp);

    if(read_edid (savedEDID)<0)
    {
       chprintf(chp, "EDID Read failed\r\n");
       return;
    }

    //Printing the captured EDID
    for(int i = 0; i < 128; i++)
    {
	     chprintf(chp, "%x ", savedEDID[i]);
    }

    chprintf(chp, "\r\n");
    BBI2C_Init (&i2cdev, GPIOC, 10, GPIOC, 11, 50000, BBI2C_MODE_SLAVE);

    //Store all captured Bytes in an array. Print after 10 captured bytes.
    chprintf (chp, "Sampling Line: \r\n");

    for (;;)
    {
      while(testval)
      {
begin:
      ackcounter = 0;
      data = BBI2C_Get_Byte (&i2cdev);
      count = 0;
      if(data == 0x6E)
      {
        data = BBI2C_Get_Byte (&i2cdev);
        if(data == 0x51)
        {
          ddcRequest = ddcci_read_master (&i2cdev);
          if (ddcRequest [2] == 0xFF)
          {
            chprintf (chp, "got invalid data for ddc/ci\r\n");
          }
          else
          {
            chprintf (chp, "ddc/ci request ok\r\n");
            uint8_t length = (ddcRequest[2] & 0x0F)+4;
            data = BBI2C_Get_Byte (&i2cdev);
            if(data == 0x6F)
            {
              signed int rc = -1;
              if(ddcRequest [4] == 0x00 && ddcRequest [5] ==  0x00)
              {
                signed int rc = ddcci_write_master (capCopy, 38);
              }
              else if(ddcRequest [4] == 0x00 && ddcRequest [5] == 0x20)
              {
                signed int rc = ddcci_write_master (capCopy1, 38);
              }
              else if(ddcRequest [4] == 0x00 && ddcRequest [5] == 0x40)
              {
                signed int rc = ddcci_write_master (capCopy2, 38);
              }
              else if(ddcRequest [4] == 0x00 && ddcRequest [5] == 0x60)
              {
                signed int rc = ddcci_write_master (capCopy3, 38);
              }
              else if(ddcRequest [4] == 0x00 && ddcRequest [5] == 0x80)
              {
                signed int rc = ddcci_write_master (capCopy4, 38);
              }
              else if(ddcRequest [4] == 0x00 && ddcRequest [5] == 0xA0)
              {
                signed int rc = ddcci_write_master (capCopy5, 38);
              }
              else if(ddcRequest [4] == 0x00 && ddcRequest [5] == 0xC0)
              {
                signed int rc = ddcci_write_master (capCopy6, 38);
              }
              else if(ddcRequest [4] == 0x00 && ddcRequest [5] == 0xE0)
              {
                signed int rc = ddcci_write_master (capCopy7, 36);
              }
              else if(ddcRequest [4] == 0x00 && ddcRequest [5] == 0xFE)
              {
                signed int rc = ddcci_write_master (capCopy8, 6);
              }
              else if(ddcRequest [4] == 0x08 && ddcRequest [5] == 0x01)
              {
                signed int rc = ddcci_write_master (capCopy9, 6);
              }
              if(rc < 0)
              {
                chprintf(chp, "no ack on bytes");
              }
              else if (rc > 0)
              {
                chprintf(chp, "ack on checksum");
              }
              else
              {
                chprintf(chp, "transmission complete");
              }
            }
          }
        }
      }
      // chprintf(chp, "gelesenes byte: %x \r\n", data);
 	   	if(data == 0xA1)
		  {
	   		   for(count = 0; count < 128; count ++)
			     {
      				ack = BBI2C_Send_Byte_To_Master ((&i2cdev), savedEDID[count]);
      				if(count <127)
      				{
                //NACK/START/STOP received
      					if(ack)
      					{
      						retry--;
      						if(retry==0)
      						{
      							return;
      						}
      						goto begin;
      					}
                //ACK received
                else
                {
                  ackcounter++;
                }
      				}
      				else
      				{
                chprintf(chp, "total acks: %d\r\n", ackcounter);
                chprintf(chp, "EDID transferred\r\n");
                testval--;
              }
      			}
			    }
	   	}
      return;
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
    if(read_edid (savedEDID)<0)
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
  signed int rcwrite, rcread;
  uint8_t retryread = 5;
  uint8_t retrywrite = 5;
  uint8_t retrycap;

  chprintf(chp, "Read EDID: \r\n");
  for(i = 0; i < retry; i++)
  {
    if(read_edid (savedEDID)<0)
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
    retryread = 10;
    retrywrite = 10;
    retrycap = 5;
/*
    rcwrite = ddcci_write_slave (capRequest, 6);
    while ((rcwrite < 0) && retrywrite)
    {
      chprintf (chp, "ddc/ci write failed\r\n");
      chThdSleepMilliseconds(50);
      rcwrite = ddcci_write_slave (capRequest, 6);
      retrywrite--;
    }
    if(rcwrite < 0)
    {
      chprintf (chp, "couldn't write to ddc/ci address\r\n");
      continue;
    }
    else
    {
      rcread = ddcci_read();
      while((rcread < 0) && retryread)
      {
        chprintf (chp, "ddc/ci read invalid - retrying\r\n");
        rcread = ddcci_read();
        retryread--;
      }
      if(rcread < 0)
      {
        chprintf (chp, "couldn't read from ddc/ci address\r\n");
        continue;
      }
    }
    chThdSleepMilliseconds (50);
  }
*/

    if(ddcci_write_slave (capRequest, 6) < 0)
    {
      chprintf(chp, "ddcciwrtie failed\r\n");
    }
    else
    {
       chprintf(chp, "ddcciwrite succeeded\r\n");
       if(ddcci_read_slave() < 0)
       {
         chprintf(chp, "failed reading ddc/ci, retrying\r\n");
         while(retrycap)
         {
           if(ddcci_write_slave (capRequest, 6) == 0)
           {
             if(ddcci_read_slave() < 0)
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
  {"ddc", cmd_ddc},
  {"pipe", cmd_pipe},
  {"sample", cmd_sample},
  {"edid", cmd_edid},
  {"pintest", cmd_pintest},
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
