#include "bbi2c.h"
#include "debug.h"

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

int BBI2C_Init
    (BBI2C_t *dev,
     stm32_gpio_t *sda_gpio,
     unsigned int sda_pin,
     stm32_gpio_t *scl_gpio,
     unsigned int scl_pin,
     unsigned long frequency,
     BBI2C_Mode_t mode)
{
    dev->sda_gpio = sda_gpio;
    dev->sda_pin  = sda_pin;
    dev->scl_gpio = scl_gpio;
    dev->scl_pin  = scl_pin;
    dev->mode     = mode;
    dev->last_scl = 1;
    dev->last_sda = 1;
    dev->state    = BS_Wait_Start;

    switch (mode)
    {
        case BBI2C_MODE_INVALID:
            return -1;
        case BBI2C_MODE_SLAVE:
            dev->delay_us = 1000000 / frequency / 4;
            break;
        case BBI2C_MODE_MASTER:
            dev->delay_us = 1000000 / frequency / 3;
            break;
        default:
            return -1;
    }

    palSetPadMode(dev->scl_gpio, dev->scl_pin, PAL_MODE_OUTPUT_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(dev->sda_gpio, dev->sda_pin, PAL_MODE_OUTPUT_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);

    Drive_SDA (dev, 1);
    Drive_SCL (dev, 1);

    return 0;
}

static BBI2C_Event_t BBI2C_Event (BBI2C_t *dev)
{
    BBI2C_Event_t result;
    int sda, scl;

    for (;;)
    {
        sda = palReadPad (dev->sda_gpio, dev->sda_pin);
        scl = palReadPad (dev->scl_gpio, dev->scl_pin);
        Delay_us (dev->delay_us);

        if (sda != dev->last_sda || scl != dev->last_scl)
        {
            if (sda)
                if (dev->last_sda)
                    result.sda = BBI2C_LEVEL_HIGH;
                else // !dev->last_sda
                    result.sda = BBI2C_LEVEL_FALL;
            else // !sda
                if (dev->last_sda)
                    result.sda = BBI2C_LEVEL_RAISE;
                else // !dev->last_sda
                    result.sda = BBI2C_LEVEL_LOW;

            if (scl)
                if (dev->last_scl)
                    result.scl = BBI2C_LEVEL_HIGH;
                else // !dev->last_scl
                    result.scl = BBI2C_LEVEL_FALL;
            else // !scl
                if (dev->last_scl)
                    result.scl = BBI2C_LEVEL_RAISE;
                else // !dev->last_scl
                    result.scl = BBI2C_LEVEL_LOW;

            dev->last_sda = sda;
            dev->last_scl = scl;
            return result;
        }
    }
}

uint8_t BBI2C_Get_Byte (BBI2C_t *dev)
{
    uint8_t result = 0;
    int count;

    for (;;)
    {
        BBI2C_Event_t event = BBI2C_Event (dev);
        
        // Go to BS_Start whenever a start condition is encountered
        if (START_CONDITION (event))
        {
            result = 0;
            count = 8;
            dev->state = BS_Start;
            continue;
        }

        switch (dev->state)
        {
            case BS_Wait_Start:
                Drive_SCL (dev, 1);
                break;

            case BS_Start:
                if (STOP_CONDITION (event))     dev->state = BS_Wait_Start;
                else if (SCL_FALLING (event))   dev->state = BS_Clock_Avail;
                break;

            case BS_Clock_Avail:
                if (SCL_RAISING (event))
                {
                    result |= (Read_SDA (dev) << count);
                    count--;
                    dev->state = BS_Data;
                }
                break;

            case BS_Data:
                if (SCL_FALLING (event))
                {
                    if (count)
                    {
                        dev->state = BS_Clock_Avail;
                    }
                    else
                    {
                        dev->state = BS_Ack;
                        Drive_SDA (dev, 0);
                    }
                }            
                break;

            case BS_Ack:
                if (SCL_RAISING (event))
                {
                    dev->state = BS_Ack_Done;
                } 
                break;

            case BS_Ack_Done:
                if (SCL_FALLING(event))
                {
                    Drive_SDA (dev, 1);
                    Drive_SCL (dev, 0);
                    dev->state = BS_Wait_Start;
                    return result;
                }
                break;
                
            default:
                break;
        }
    }
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
