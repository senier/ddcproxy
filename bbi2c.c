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

#include "bbi2c.h"
#include "debug.h"
#include "usbcfg.h"

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

void Check_Stretch_SCL (BBI2C_t *dev)
{
    int clock = 0;
    while (clock == 0){
    	clock = Read_SCL (dev);
    }
   // Drive_SCL (dev, 1);
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
                    result.sda = BBI2C_LEVEL_RAISE;
            else // !sda
                if (dev->last_sda)
                    result.sda = BBI2C_LEVEL_FALL;
                else // !dev->last_sda
                    result.sda = BBI2C_LEVEL_LOW;

            if (scl)
                if (dev->last_scl)
                    result.scl = BBI2C_LEVEL_HIGH;
                else // !dev->last_scl
                    result.scl = BBI2C_LEVEL_RAISE;
            else // !scl
                if (dev->last_scl)
                    result.scl = BBI2C_LEVEL_FALL;
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
    typedef struct
    {
      BBI2C_State_t state;
      BBI2C_Level_t sda;
      BBI2C_Level_t scl;
    } debug_t;

    BBI2C_State_t oldstate = 0;

    uint8_t result = 0;
    int count = 8;
    debug_t transitions[15];
    uint32_t tcounter = 0;

    for (;;)
    {
        BBI2C_Event_t event = BBI2C_Event (dev);

	//if (tcounter < (sizeof(transitions)/sizeof(debug_t)))
	//{
	//  if(oldstate!=dev->state || dev->state!=BS_Wait_Start){
	//     transitions[tcounter].state = dev->state;
        //     transitions[tcounter].sda = event.sda;
	//    transitions[tcounter].scl = event.scl;
	//     tcounter++;
        //  }
	//} else {
	//     	for(tcounter = 0; tcounter < 15; tcounter++)
	//	{
	//		chprintf(&SDU1,"%d/%d/%d ", transitions[tcounter].state, transitions[tcounter].sda, transitions[tcounter].scl);
	//	}
        //   	tcounter = 0;
        // 	}
        //oldstate = dev->state;

	// Go to BS_Start whenever a start condition is encountered
        if (START_CONDITION (event))
        {
            result = 0;
            count = 8;
            dev->state = BS_Start;
            continue;
        }
	if (STOP_CONDITION (event))
	{
	    dev->state = BS_Wait_Start;
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
                    result |= (Read_SDA (dev) << (count-1));
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
                    dev->state = BS_Clock_Avail;
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
    Delay_us (dev->delay_us);
    Drive_SCL (dev, 1);
    Delay_us (dev->delay_us);
    Drive_SCL (dev, 0);
    Drive_SDA (dev, 1);
    Delay_us (dev->delay_us);
}

void BBI2C_NACK (BBI2C_t *dev)
{
	Drive_SDA (dev, 1);
    Delay_us (dev->delay_us);
    Drive_SCL (dev, 1);
    Delay_us (dev->delay_us);
    Drive_SCL (dev, 0);
    Delay_us (dev->delay_us);
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
     Check_Stretch_SCL (dev);

     return (ack_bit == 0);
}

void BBI2C_Recv_Byte (BBI2C_t *dev, uint8_t *result)
{
    int i;
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
            *result = *result << 1;
        }

        Drive_SCL (dev, 0);
        Delay_us (dev->delay_us);
    }
   	return;
}

int BBI2C_Send_Byte_To_Master (BBI2C_t *dev, uint8_t data)
{
    unsigned char i, ack_bit;
/*
    for(;;)
    {
	if(Read_SCL (dev))
	{
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
        	    data <<= 1;
		    for(;;)
		    {
			if(!Read_SCL (dev)) break;
		    }
		    if(i==7) goto finish;
		}
	}
    }

finish:
    for(;;)
    {
	if(Read_SCL (dev)) break;
    }
    ack_bit = Read_SDA (dev);
    return (ack_bit == 0);
}  */

    uint8_t oData = data;
    uint8_t result = 0;
    int count = 8;

    dev->state = BS_Clock_Avail;
    for (;;)
    {
        BBI2C_Event_t event = BBI2C_Event (dev);

    	// Go to BS_Start whenever a start condition is encountered
       if (START_CONDITION (event))
       {
    	    return 3;
       }
       if (STOP_CONDITION (event))
       {
    	    return 2;
       }

      switch (dev->state)
      {
          case BS_Clock_Avail:
              if (SCL_LOW (event)||SCL_FALLING(event))
              {
    		          if (count)
                  {
    			             if ((data & 0x80) == 0)
        	   	        {
           				 	       Drive_SDA (dev, 0);
           		    	  }
           		    	  else
           		        {
           		        		Drive_SDA (dev, 1);
           		    	  }
            	        count--;
                      dev->state = BS_Data;
           		        data <<= 1;
    		     		  }
    		     		  else
    		     		  {
    					           dev->state = BS_Ack;
    		     		  }
              }
              break;

          case BS_Data:
              if (SCL_FALLING (event))
    	        {
               		dev->state = BS_Clock_Avail;
              }
              break;

          case BS_Ack:
              if (SCL_RAISING (event))
              {
	    		        ack_bit = Read_SDA (dev);
	    		        return ack_bit;
              }
              break;

          default:
              break;
        }
      }
}
