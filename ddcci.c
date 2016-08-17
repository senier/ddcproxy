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

  BBI2C_t dev;
  BBI2C_Init (&dev, GPIOC, 4, GPIOC, 5, 50000, BBI2C_MODE_MASTER);

  do {
    BBI2C_Start (&dev);
    ack = BBI2C_Send_Byte (&dev, DEFAULT_EDID_R_ADDR);
    if(ack)
  	{
  		for(k = 0; k < 128; k++)
  		{
  			BBI2C_Recv_Byte (&dev, &edid[k]);
  			if (k==127) BBI2C_NACK (&dev);
  			else BBI2C_Ack (&dev);
  		}
      BBI2C_Stop(&dev);
      return 0;
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
