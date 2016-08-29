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

uint8_t edidstring[18] = /* writing 'owned' as the display name string */
{0x6F, 0x77, 0x6E, 0x65, 0x64,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};

#define EDID_LENGTH 128
#define RAND_MAX 255

int rand(void);

uint8_t * edid_monitor_string_faker (uint8_t *edid)
{
  static uint8_t newEdid[128];
  uint8_t i, k;
  uint8_t length = 128;
  uint32_t sum = 0;
  uint8_t checksum = 0;

  chprintf(&SDU1, "changing edid\r\n");

  for(i = 0; i < 128; i++)
  {
    newEdid[i] = edid[i];
  }

  for(i = 54; i < length; i++)
  {
    if(edid[i] == 0x00)
    {
      if(edid[i+1] == 0x00 && edid[i+1] == 0x00 && edid[i+2] == 0xFC &&
          edid[i+3] == 0x00)
          {
            chprintf(&SDU1, "found sequence\r\n");
            for(k = 0; k < 13; k++)
            {
              newEdid[i+4] = edidstring[k];
              i++;
            }
            break;
          }
    }
  }

  /* calculate checksum */
  for(i = 0; i < 127; i++)
  {
    sum += newEdid[i];
  }
  checksum = 256 - (sum % 256);
  newEdid[127] = checksum;
  sum = 0;


  chprintf(&SDU1, "faked edid:\r\n");
  for (i = 0; i < 128; i++)
  {
    chprintf(&SDU1, "%02x ", newEdid[i]);
  }
  chprintf(&SDU1, "\r\n");
  return newEdid;
}

uint8_t * edid_fuzzer_unary (uint8_t *savedEDID)
{
  static uint8_t edid[128];
  uint8_t element, value, i;
  uint32_t sum = 0;
  uint8_t checksum = 0;

  element = rand() % 127;
  value = rand() % 256;

  savedEDID[element] = value;

  /* calculate checksum */
  for(i = 0; i < 127; i++)
  {
    sum += edid[i];
  }
  checksum = 256 - (sum % 256);
  edid[127] = checksum;
  sum = 0;

  chprintf(&SDU1, "Changed Byte Number %d to %02x", element, value);

  return edid;
}

uint8_t * edid_fuzzer_complete (void)
{
  static uint8_t edid[128];
  uint8_t value, i;
  uint32_t sum = 0;
  uint8_t checksum = 0;

  for(i = 0; i < 127; i++)
  {
    value = rand() % 256;
    edid[i] = value;
  }

  /* calculate checksum */
  for(i = 0; i < 127; i++)
  {
    sum += edid[i];
  }
  checksum = 256 - (sum % 256);
  edid[127] = checksum;
  sum = 0;

  return edid;
}
