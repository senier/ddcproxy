#ifndef BBI2C_H
#define BBI2C_H

#include "hal.h"

typedef struct
{
    stm32_gpio_t *sda_gpio;
    int sda_pin;
    stm32_gpio_t *scl_gpio;
    int scl_pin;
    unsigned long delay_us;
} BBI2C_t;

void BBI2C_Init (BBI2C_t *dev, stm32_gpio_t *sda_gpio, unsigned int sda_pin, stm32_gpio_t *scl_gpio, unsigned int scl_pin, unsigned long frequency);
void BBI2C_Start (BBI2C_t *dev);
void BBI2C_Restart (BBI2C_t *dev);
void BBI2C_Stop (BBI2C_t *dev);
void BBI2C_ACK (BBI2C_t *dev);
void BBI2C_NACK (BBI2C_t *dev);

int BBI2C_Send_Byte (BBI2C_t *dev, uint8_t data);
int BBI2C_Recv_Byte (BBI2C_t *dev, uint8_t *data);

#endif // BBI2C_H
