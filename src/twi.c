/*!
 *******************************************************************************
 * @file    twi.c
 *
 * @date    February 11, 2013
 *
 * @author  Kaharman
 *
 * @brief   Two Wire Interface (TWI) functions
 *******************************************************************************
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <util/twi.h>

/*******************************************************************************
 * Functions
 ******************************************************************************/
void TWIMasterInitialize()
{
    TWBR = TWI_TWBR;                                /* set bit rate register  */
    TWDR = 0xFF;                                    /* SDA released           */
    TWCR = (1 << TWEN) |                            /* enable TWI interface   */
           (0 << TWIE) | (0 << TWINT) |             /* disable interrupt      */
           (0 << TWEA) | (0 << TWSTA) | (0 << TWSTO) |  /* no signal requests */
           (0 << TWWC);
}

unsigned char TWITransceiverBusy()
{
    /* if TWI interrupt is enabled then the transceiver is busy */
    return (TWCR & (1 << TWIE));
}

unsigned char TWIGetStateInfo(void)
{
    while (TWITransceiverBusy());               /* wait until TWI has         */
                                                /* completed the transmission */

    return (TWI_state);                         /* return error state         */
}

void TWIStartTransceiverWithData(unsigned char *msg, unsigned char msgSize)
{
    unsigned char temp;
    
    while (TWITransceiverBusy());               /* wait until TWI is ready    */
                                                /* for next transmission      */
    
    TWI_msgSize = msgSize;                      /* number of data to transmit */
    TWI_buf[0] = msg[0];                        /* store slave address (R/W)  */
    if (!(msg[0] & (TRUE << TWI_READ_BIT)))     /* if it is a write operation */
    {                                           /* then also copy data        */
        for (temp = 1; temp < msgSize; temp++)
        {
            TWI_buf[temp] = msg[temp];
        }
    }
    TWI_statusReg.all = 0;
    TWI_state = TWI_NO_STATE;
    TWCR = (1 << TWEN) |                        /* TWI interface enabled      */
           (1 << TWIE) | (1 << TWINT) |         /* enable TWI int, clear flag */
           (0 << TWEA) | (1 << TWSTA) | (0 << TWSTO) |  /* initiate a START   */
           (0 << TWWC);
}
