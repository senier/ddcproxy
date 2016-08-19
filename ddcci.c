/*
 * Copyright (c) 2016, Daniel Matusek <daniel.matusek@tu-dresden.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "ch.h"
#include "hal.h"
#include "usbcfg.h"
#include "bbi2c.h"
#include "debug.h"
#include "ddcci.h"

#include "shell.h"
#include "chprintf.h"

#define DEFAULT_DDCCI_ADDR 0x6E
#define DEFAULT_EDID_W_ADDR 0xA0
#define DEFAULT_EDID_R_ADDR 0xA1

int ddcci_write(BBI2C_t *dev)
{

}

int ddcci_read(BBI2C_t *dev)
{

}

int read_edid(uint8_t *edid)
{

  uint8_t ack, k;
  uint8_t retry = 3;
  uint8_t cycle = 1;

  BBI2C_t dev;
  BBI2C_Init (&dev, GPIOC, 4, GPIOC, 5, 50000, BBI2C_MODE_MASTER);

  do {
    BBI2C_Start (&dev);
    ack = BBI2C_Send_Byte (&dev, DEFAULT_EDID_R_ADDR);
    chThdSleepMicroseconds(5);
    if(ack)
  	{
  		for(k = 0; k < 128; k++)
  		{
  			BBI2C_Recv_Byte (&dev, &edid[k]);
        if (k < 7) BBI2C_Ack (&dev);
        else if(k > 6 && edid[k-7]==0x00 && edid[k]==0x00 && edid[k-5]==0xFF && edid[k-4]==0xFF && edid[k-3]==0xFF
        && edid[k-2]==0xFF && edid[k-1]==0xFF && edid[k-6]==0xFF)
        {
            chprintf(&SDU1, "found header!\r\n");
            BBI2C_NACK (&dev);
            BBI2C_Stop (&dev);
            edid[0] = 0x00;
            edid[7] = 0x00;
            edid[1] = 0xFF;
            edid[2] = 0xFF;
            edid[3] = 0xFF;
            edid[4] = 0xFF;
            edid[5] = 0xFF;
            edid[6] = 0xFF;
            k=7;
            BBI2C_Start (&dev);
            BBI2C_Send_Byte (&dev, DEFAULT_EDID_R_ADDR);
        }
        else if (k==127 && edid[0]==0x00 && edid[7]==0x00 && edid[1]==0xFF && edid[2]==0xFF && edid[3]==0xFF
        && edid[4]==0xFF && edid[5]==0xFF && edid[6]==0xFF)
        {
          BBI2C_NACK (&dev);
          BBI2C_Stop (&dev);
          return 0;
        }
        else if (k < 127) BBI2C_Ack (&dev);
        else
        {
          if(cycle)
          {
            BBI2C_NACK (&dev);
            BBI2C_Stop (&dev);
            BBI2C_Start (&dev);
            BBI2C_Send_Byte (&dev, DEFAULT_EDID_R_ADDR);
            k = 0;
            cycle = 0;
          }
          else return -1;
        }
  		}
  	}
  	else
  	{
  		BBI2C_NACK(&dev);
  		BBI2C_Stop(&dev);
      retry--;
  	}
  } while(retry);

  return -1;
}
