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
 * Defines
 ******************************************************************************/
#define TRUE            1
#define FALSE           0

#define TWI_READ_BIT    0               /* Bit position for R/W bit           */
#define TWI_ADR_BITS    1               /* Bit position for LSB               */

#define TWI_BUFFER_SIZE 4               /* set this to the largest message    */
                                        /*   size that will be sent           */
#define TWI_TWBR        0x0c            /* TWI Bit rate Register setting      */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#ifdef COMPILE
#include <avr/io.h>
#else
#include <avr/iom8.h>
#endif
#include <avr/interrupt.h>
#include <util/twi.h>

/*******************************************************************************
 * Global Variables
 ******************************************************************************/
static unsigned char TWI_buf[TWI_BUFFER_SIZE];  /* Transceiver buffer         */
static unsigned char TWI_msgSize;               /* NumOfBytes to be transmit  */
static unsigned char TWI_state = TW_NO_INFO;    /* State byte                 */

union TWI_statusReg                             /* Status byte holding flags  */
{
    unsigned char all;
    struct
    {
        unsigned char lastTransOK:1;
        unsigned char unusedBits:7;
    };
};

union TWI_statusReg TWI_statusReg = {0};

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
                                                /*   for next transmission    */
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
    TWI_state = TW_NO_INFO;
    TWCR = (1 << TWEN) |                        /* TWI interface enabled      */
           (1 << TWIE) | (1 << TWINT) |         /* enable TWI int, clear flag */
           (0 << TWEA) | (1 << TWSTA) | (0 << TWSTO) |  /* initiate a START   */
           (0 << TWWC);
}

void TWIStartTransceiver()
{
    while (TWITransceiverBusy());               /* wait until TWI is ready    */
                                                /*   for next transmission    */
    TWI_statusReg.all = 0;
    TWI_state = TW_NO_INFO;
    TWCR = (1 << TWEN) |                        /* TWI interface enabled      */
           (1 << TWIE) | (1 << TWINT) |         /* enable TWI int, clear flag */
           (0 << TWEA) | (1 << TWSTA) | (0 << TWSTO) |  /* initiate a START   */
           (0 << TWWC);
}

unsigned char TWIGetDataFromTransceiver(unsigned char *msg,
                                        unsigned char msgSize)
{
    unsigned char i;

    while (TWITransceiverBusy());               /* wait until TWI is ready    */
                                                /*   for next transmission    */
    if (TWI_statusReg.lastTransOK)              /* last transmission competed */
    {
        for (i = 0; i < msgSize; i++)           /* copy data from Transceiver */
        {                                       /*    buffer                  */
            msg[i] = TWI_buf[i];
        }
    }
    return (TWI_statusReg.lastTransOK);
}

ISR(TWI_vect)
{
    static unsigned char TWI_bufPtr;

    switch (TWSR)
    {
    case TW_START:             // START has been transmitted
    case TW_REP_START:         // Repeated START has been transmitted
        TWI_bufPtr = 0;                                     // Set buffer pointer to the TWI Address location
    case TW_MT_SLA_ACK:       // SLA+W has been tramsmitted and ACK received
    case TW_MT_DATA_ACK:      // Data byte has been tramsmitted and ACK received
        if (TWI_bufPtr < TWI_msgSize)
        {
            TWDR = TWI_buf[TWI_bufPtr++];
            TWCR = (1 << TWEN) |                                 // TWI Interface enabled
                   (1 << TWIE) | (1 << TWINT) |                      // Enable TWI Interupt and clear the flag to send byte
                   (0 << TWEA) | (0 << TWSTA) | (0 << TWSTO) |           //
                   (0 << TWWC);                                 //
        }
        else                    // Send STOP after last byte
        {
            TWI_statusReg.lastTransOK = TRUE;                 // Set status bits to completed successfully.
            TWCR = (1 << TWEN) |                                 // TWI Interface enabled
                   (0 << TWIE) | (1 << TWINT) |                      // Disable TWI Interrupt and clear the flag
                   (0 << TWEA) | (0 << TWSTA) | (1 << TWSTO) |           // Initiate a STOP condition.
                   (0 << TWWC);                                 //
        }
        break;
    case TW_MR_DATA_ACK:      // Data byte has been received and ACK tramsmitted
        TWI_buf[TWI_bufPtr++] = TWDR;
    case TW_MR_SLA_ACK:       // SLA+R has been tramsmitted and ACK received
        if (TWI_bufPtr < (TWI_msgSize - 1))                  // Detect the last byte to NACK it.
        {
            TWCR = (1 << TWEN) |                                 // TWI Interface enabled
                   (1 << TWIE) | (1 << TWINT) |                      // Enable TWI Interupt and clear the flag to read next byte
                   (1 << TWEA) | (0 << TWSTA) | (0 << TWSTO) |           // Send ACK after reception
                   (0 << TWWC);                                 //
        }
        else                    // Send NACK after next reception
        {
            TWCR = (1 << TWEN) |                                 // TWI Interface enabled
                   (1 << TWIE) | (1 << TWINT) |                      // Enable TWI Interupt and clear the flag to read next byte
                   (0 << TWEA) | (0 << TWSTA) | (0 << TWSTO) |           // Send NACK after reception
                   (0 << TWWC);                                 //
        }
        break;
    case TW_MR_DATA_NACK:     // Data byte has been received and NACK tramsmitted
        TWI_buf[TWI_bufPtr] = TWDR;
        TWI_statusReg.lastTransOK = TRUE;                 // Set status bits to completed successfully.
        TWCR = (1 << TWEN) |                                 // TWI Interface enabled
               (0 << TWIE) | (1 << TWINT) |                      // Disable TWI Interrupt and clear the flag
               (0 << TWEA) | (0 << TWSTA) | (1 << TWSTO) |           // Initiate a STOP condition.
               (0 << TWWC);                                 //
        break;
    case TW_MT_ARB_LOST:          // Arbitration lost
        TWCR = (1 << TWEN) |                                 // TWI Interface enabled
               (1 << TWIE) | (1 << TWINT) |                      // Enable TWI Interupt and clear the flag
               (0 << TWEA) | (1 << TWSTA) | (0 << TWSTO) |           // Initiate a (RE)START condition.
               (0 << TWWC);                                 //
        break;
    case TW_MT_SLA_NACK:      // SLA+W has been tramsmitted and NACK received
    case TW_MR_SLA_NACK:      // SLA+R has been tramsmitted and NACK received
    case TW_MT_DATA_NACK:     // Data byte has been tramsmitted and NACK received
    //    case TWI_NO_STATE              // No relevant state information available; TWINT = �0�
    case TW_BUS_ERROR:         // Bus error due to an illegal START or STOP condition
    default:
        TWI_state = TWSR;                                 // Store TWSR and automatically sets clears noErrors bit.
                                                        // Reset TWI Interface
        TWCR = (1 << TWEN) |                                 // Enable TWI-interface and release TWI pins
               (0 << TWIE) | (0 << TWINT) |                      // Disable Interupt
               (0 << TWEA) | (0 << TWSTA) | (0 << TWSTO) |           // No Signal requests
               (0 << TWWC);                                 //
        break;
    }
}
