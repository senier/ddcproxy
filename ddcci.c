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

//ADDRESSES
#define DEFAULT_EDID_W_ADDR 0xA0
#define DEFAULT_EDID_R_ADDR 0xA1
#define DEFAULT_DDCCI_ADDR 0x6E
#define DEFAULT_DDCCI_R_ADDR 0x6F

//OTHER FIXED VALUES
#define DEFAULT_DDCCI_LENGTH 0x80
#define DDCCI_RECEIVE_INITIAL_CHK 0x51

#define EDID_LENGTH 128

void BBI2C_Ack (BBI2C_t *dev);

int ddcci_write_slave(uint8_t *stream, uint8_t len)
{
    uint8_t i, ack;
    uint8_t send = 1;

    BBI2C_t dev;
    BBI2C_Init (&dev, GPIOC, 4, GPIOC, 5, 50000, BBI2C_MODE_MASTER);
    BBI2C_Start (&dev);

    for(i = 0; i < len; i++)
    {
      ack = BBI2C_Send_Byte (&dev, stream[i]);
      if(!ack)
      {
        BBI2C_Stop (&dev);
        return -1;
      }
    }

    ack = BBI2C_Send_Byte (&dev, checksum(send, stream, len));
    if(!ack)
    {
      BBI2C_Stop (&dev);
      return -1;
    }

    BBI2C_Stop (&dev);
    return 0;
}

int ddcci_write_master(uint8_t *stream, uint8_t len, uint8_t fakeChk)
{
  uint8_t i, ack, chk;
  BBI2C_t dev;
  BBI2C_Init (&dev, GPIOC, 10, GPIOC, 11, 50000, BBI2C_MODE_SLAVE);

  for(i = 0; i < len; i++)
  {
    ack = BBI2C_Send_Byte_To_Master (&dev, stream[i]);
    if (ack == 0)
    {
      continue;
    }
    else return -1;
  }
  chk = checksum (0, stream, len);
  if(fakeChk) chk = 0x00;
  ack = BBI2C_Send_Byte_To_Master (&dev, chk);
  if(ack == 1) return 0;
  else return 1;

}

int ddcci_read_slave(uint8_t *result)
{

  chThdSleepMilliseconds (40);
  BBI2C_t dev;
  BBI2C_Init (&dev, GPIOC, 4, GPIOC, 5, 10000, BBI2C_MODE_MASTER);
  BBI2C_Start (&dev);

  //uint8_t result[128];
  uint8_t i, ack;
  uint8_t msg_length = 0;
  uint8_t chk;

  ack = BBI2C_Send_Byte (&dev, DEFAULT_DDCCI_R_ADDR);
  if(!ack)
  {
     chprintf(&SDU1, "no ack on 6f while reading \r\n");
     return -1;
  }
  chThdSleepMicroseconds(5);

  BBI2C_Recv_Byte (&dev, &result[0]);
  BBI2C_Ack (&dev);
  chThdSleepMicroseconds(5);

  BBI2C_Recv_Byte (&dev, &result[1]);
  BBI2C_Ack (&dev);
  msg_length = result[1] & 0x7F;
  chThdSleepMicroseconds(5);

  if(checkNullMessage (result[1])) /* Null message */
  {
    msg_length = 0;
    chprintf(&SDU1, "nullmessage from monitor\r\n");
  }
  else if (msg_length > 35)/* length only 3-35 as defined in vesa ddc/di doc */
  {
    chprintf(&SDU1, "invalid message length, got %02x \r\n", result[1]);
    BBI2C_NACK (&dev);
    BBI2C_Stop (&dev);
    return -1;
  }
  else /* Not a null message and valid fragment length */
  {
    chprintf(&SDU1, "length of ddc/ci message: %d \r\n", msg_length);
    for(i = 0; i < msg_length; i++)
    {
      BBI2C_Recv_Byte (&dev, &result[i+2]);
      BBI2C_Ack (&dev);
      chThdSleepMicroseconds(5);
    }
  }

  BBI2C_Recv_Byte (&dev, &result[msg_length+2]);
  BBI2C_NACK (&dev);
  BBI2C_Stop (&dev);

  chk = checksum(0, result, (msg_length+1));
  if(chk != result[msg_length+2]) return -1;
  chprintf(&SDU1, "calculated chksum %02x\r\n", chk);

  for(i = 0; i < (msg_length+3); i++)
  {
    chprintf(&SDU1, "%02x ", result[i]);
  }
  chprintf(&SDU1, "\r\n");
  return 0;
}

uint8_t * ddcci_read_master (BBI2C_t *dev, uint8_t len)
{
  uint8_t data, i, chk, fragment_length;
  static uint8_t result[128];
  result[0] = 0x6E;
  result[1] = 0x51;
  result[2] = len;

  fragment_length = len & 0x0F;

  for(i = 0; i < fragment_length; i++)
  {
    data = BBI2C_Get_Byte (dev);
    result[i+3] = data; /* +3 offset because of 0x6E, 0x51, 0x8X */
  }

  data = BBI2C_Get_Byte (dev);
  chk = checksum (1, result, len+2);
  if(chk != data)
  {
     result[1]=0xFF;
  }
  result[len+3] = chk;

  return result;
}

uint8_t * read_edid()
{

  uint8_t ack, k;
  static uint8_t edid[128];
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
          return edid;
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
          else
          {
            edid[0] = 0xFF;
            return edid;
          }
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

  edid[0] = 0xFF;
  return edid;
}

int write_edid (BBI2C_t i2cdev01, uint8_t *edid)
{
  uint8_t i, ack;

  chprintf(&SDU1, "sending edid\r\n");

  for (i = 0; i < EDID_LENGTH; i++)
  {
    ack = BBI2C_Send_Byte_To_Master (&i2cdev01, edid[i]);
      if (ack == 0) continue;
      else if (i == 127)
      {
        if (ack == 1) return 0;
      }
      else
      {
        chprintf(&SDU1, "NACK on %02x \r\n", edid[i]);
        return -1;
      }
    }
  return 0;
}

uint8_t checksum (uint8_t send, uint8_t stream[], uint8_t len)
{
  uint8_t i = 0;
  uint8_t checksum = 0;
  if(send)
  {
      for (i = 0; i < len; i++)
      {
        checksum ^= stream[i]; /* all but first, it's already initialized */
      }
      return checksum;
  }
  else /* receving checksum is calculated different from sending checksum */
  {
    checksum = DEFAULT_DDCCI_R_ADDR; /* take 0x6f */
    checksum ^= DDCCI_RECEIVE_INITIAL_CHK; /* for receving: second byte 0x51 */
    for  (i = 1; i < len+1; i++)
    {
        checksum ^= stream[i];
    }
    return checksum;
  }
}

uint8_t checkNullMessage (uint8_t val)
{
  if (val == 0x80) return 1;
  else return 0;
}
