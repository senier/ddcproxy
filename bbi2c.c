#include "bbi2c.h"

static inline void Delay_us (uint32_t interval)
{
    uint32_t iterations = interval * 8;
    for (uint32_t i = 0; i < iterations; i++)
    {
        __asm__ volatile
        (
            "nop\n\t"
            :::
        );
    }
}

int Read_SDA (BBI2C_t *dev)
{
    int result;
    result = palReadPad (dev->sda_gpio, dev->sda_pin);
    return result;
};

int Read_SCL (BBI2C_t *dev)
{
    return palReadPad (dev->scl_gpio, dev->scl_pin);
};

void Drive_SDA (BBI2C_t *dev, int sda)
{
    if (sda)
    {
        palSetPad (dev->sda_gpio, dev->sda_pin);
    }
    else
    {
        palClearPad (dev->sda_gpio, dev->sda_pin);
    }
}

void Drive_SCL (BBI2C_t *dev, int scl)
{
    if (scl)
    {
        palSetPad (dev->scl_gpio, dev->scl_pin);
    }
    else
    {
        palClearPad (dev->scl_gpio, dev->scl_pin);
    }
}

void Release_SCL (BBI2C_t *dev)
{
    Drive_SCL (dev, 1);
    while (!Read_SCL (dev));
}

void BBI2C_Init
    (BBI2C_t *dev,
     stm32_gpio_t *sda_gpio,
     unsigned int sda_pin,
     stm32_gpio_t *scl_gpio,
     unsigned int scl_pin,
     unsigned long frequency)
{
    dev->sda_gpio = sda_gpio;
    dev->sda_pin  = sda_pin;
    dev->scl_gpio = scl_gpio;
    dev->scl_pin  = scl_pin;
    dev->delay_us = 1000000 / frequency / 3;

    palSetPadMode(dev->scl_gpio, dev->scl_pin, PAL_MODE_OUTPUT_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(dev->sda_gpio, dev->sda_pin, PAL_MODE_OUTPUT_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);

    Drive_SDA (dev, 1);
    Release_SCL (dev);
}

void BBI2C_Start (BBI2C_t *dev)
{
    Release_SCL (dev);
    Drive_SDA (dev, 1);
    Delay_us (dev->delay_us);
    Drive_SDA (dev, 0);
    Delay_us (dev->delay_us);
}

void BBI2C_Stop (BBI2C_t *dev)
{
    Drive_SDA (dev, 0);
    Delay_us (dev->delay_us);
    Release_SCL (dev);
    Delay_us (dev->delay_us);
    Drive_SDA (dev, 1);
    Delay_us (dev->delay_us);
}

void BBI2C_Ack (BBI2C_t *dev)
{
    Drive_SDA (dev, 0);
    Drive_SCL (dev, 1);
    Drive_SCL (dev, 0);
    Drive_SDA (dev, 1);
}

void BBI2C_NACK (BBI2C_t *dev)
{
    Drive_SDA (dev, 1);
    Drive_SCL (dev, 1);
    Drive_SCL (dev, 0);
    Drive_SDA (dev, 0);
}

int BBI2C_Send_Byte (BBI2C_t *dev, uint8_t data)
{
    Drive_SCL (dev, 0);

    unsigned char i, ack_bit;
    for (i = 0; i < 8; i++)
    {
        if ((data & 0x80) == 0)
        {
            Drive_SDA (dev, 0);
        }
        else
        {
            Drive_SDA (dev, 1);
        }

        Delay_us (dev->delay_us);
        Release_SCL (dev);
        Delay_us (dev->delay_us);
        Drive_SCL (dev, 0);
        Delay_us (dev->delay_us);
        data <<= 1;
     }

     Drive_SDA (dev, 1);
     Delay_us (dev->delay_us);
     Release_SCL (dev);
     ack_bit = Read_SDA (dev);

     Delay_us (dev->delay_us);
     Drive_SCL (dev, 0);
     ack_bit = Read_SDA (dev);

     return (ack_bit == 0);
}

int BBI2C_Recv_Byte (BBI2C_t *dev, uint8_t *result)
{
    int i, ack_bit;

    *result = 0;
    Drive_SCL (dev, 0);

    for (i = 0; i < 8; i++)
    {
        Delay_us (dev->delay_us);
        Release_SCL (dev);
        Delay_us (dev->delay_us);

        if (Read_SDA (dev))
        {
            *result |=1;
        }
        if (i < 7)
        {
            *result <<= 1;
        }

        Drive_SCL (dev, 0);
        Delay_us (dev->delay_us);
    }

    Drive_SDA (dev, 1);
    Delay_us (dev->delay_us);
    Release_SCL (dev);
    ack_bit = Read_SDA (dev);

    Delay_us (dev->delay_us);
    Drive_SCL (dev, 0);
    ack_bit = Read_SDA (dev);

    return (ack_bit == 0);
}
