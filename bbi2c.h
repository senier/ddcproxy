/*
 * Copyright (c) 2016, Alexander Senier <alexander.senier@tu-dresden.de>
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

#ifndef BBI2C_H
#define BBI2C_H

#include "hal.h"

typedef enum
{
    BBI2C_MODE_INVALID,
    BBI2C_MODE_SLAVE,
    BBI2C_MODE_MASTER
} BBI2C_Mode_t;

typedef enum
{
    BBI2C_LEVEL_LOW = 1,
    BBI2C_LEVEL_HIGH = 2,
    BBI2C_LEVEL_RAISE = 3,
    BBI2C_LEVEL_FALL = 4
} BBI2C_Level_t;

typedef struct
{
    BBI2C_Level_t sda;
    BBI2C_Level_t scl;
} BBI2C_Event_t;

#define SDA_RAISING(ev) ev.sda == BBI2C_LEVEL_RAISE
#define SDA_FALLING(ev) ev.sda == BBI2C_LEVEL_FALL
#define SDA_HIGH(ev)    ev.sda == BBI2C_LEVEL_HIGH
#define SDA_LOW(ev)     ev.sda == BBI2C_LEVEL_LOW
#define SCL_RAISING(ev) ev.scl == BBI2C_LEVEL_RAISE
#define SCL_FALLING(ev) ev.scl == BBI2C_LEVEL_FALL
#define SCL_HIGH(ev)    ev.scl == BBI2C_LEVEL_HIGH
#define SCL_LOW(ev)     ev.scl == BBI2C_LEVEL_LOW
#define SDA_VAL(ev)     (SDA_HIGH(ev) || SDA_RAISING(ev))

#define START_CONDITION(ev) SDA_FALLING (ev) && SCL_HIGH (ev)
#define STOP_CONDITION(ev)  SDA_RAISING (ev) && SCL_HIGH (ev)

typedef enum
{
    BS_Wait_Start  = 1,
    BS_Start       = 2,
    BS_Clock_Avail = 3,
    BS_Data        = 4,
    BS_Ack         = 5,
    BS_Ack_Done    = 6
} BBI2C_State_t;

typedef struct
{
    stm32_gpio_t *sda_gpio;
    int sda_pin;
    stm32_gpio_t *scl_gpio;
    int scl_pin;
    unsigned long delay_us;
    BBI2C_Mode_t mode;
    int last_scl;
    int last_sda;
    BBI2C_State_t state;
} BBI2C_t;

int BBI2C_Init
    (BBI2C_t *dev,
     stm32_gpio_t *sda_gpio,
     unsigned int sda_pin,
     stm32_gpio_t *scl_gpio,
     unsigned int scl_pin,
     unsigned long frequency,
     BBI2C_Mode_t mode);

void BBI2C_Start (BBI2C_t *dev);
void BBI2C_Restart (BBI2C_t *dev);
void BBI2C_Stop (BBI2C_t *dev);
void BBI2C_ACK (BBI2C_t *dev);
void BBI2C_NACK (BBI2C_t *dev);

int BBI2C_Send_Byte (BBI2C_t *dev, uint8_t data);
void BBI2C_Recv_Byte (BBI2C_t *dev, uint8_t *data);
int BBI2C_Send_Byte_To_Master (BBI2C_t *dev, uint8_t data);

uint8_t BBI2C_Get_Byte (BBI2C_t *dev);

#endif // BBI2C_H
