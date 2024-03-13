#include "mailbox.h"

#include "mini_uart.h"
#include "peripherals/base.h"
#include "peripherals/gpio.h"


#define VIDEOCORE_MBOX (PBASE + 0x0000B880)
#define MBOX_READ      ((volatile unsigned int*) (VIDEOCORE_MBOX + 0x0))
#define MBOX_STATUS    ((volatile unsigned int*) (VIDEOCORE_MBOX + 0x18))
#define MBOX_WRITE     ((volatile unsigned int*) (VIDEOCORE_MBOX + 0x20))
#define MBOX_RESPONSE  0x80000000   // response code: request successful
#define MBOX_FULL      0x80000000
#define MBOX_EMPTY     0x40000000

/* mailbox message buffer */
volatile unsigned int __attribute__((aligned(16))) mbox[36];

void prepare_mbox_message(unsigned int tag, unsigned int buffer_size) {
    // Common header
    mbox[0] = buffer_size;    // Total message size in bytes
    mbox[1] = MBOX_REQUEST;   // This is a request message
    // tags begin
    mbox[2] = tag;   // tag identifier
    mbox[3] = 8;     // maximum of request and response value buffer's length.
    mbox[4] = 8;   // tag request code 
    mbox[5] = 0;   // value buffer
    mbox[6] = 0;   // value buffer
    // tags end
    mbox[7] = MBOX_TAG_LAST;
}

/**
 * Make a mailbox call to specified channel. Returns 0 on failure, non-zero on success
 */
int mbox_call(unsigned char ch) {
    /* clear the lowest four bits, ensure the address is 16 bits aligned*/
    unsigned int r = (((unsigned int) ((unsigned long) &mbox) & ~0xF) | (ch & 0xF));
    /* check MBOX_STATUS register is full(MBOX_FULL) or not, if full then loop waiting until we can
     * write to the mailbox */
    while (1) {
        if (!(*MBOX_STATUS & MBOX_FULL))
            break;
    }
    /* write the address of our message to the mailbox with channel identifier */
    *MBOX_WRITE = r; // read channel
    /* now wait for the response */
    while (1) {
        /* is there a response? */
        while (1) {
            if (!(*MBOX_STATUS & MBOX_EMPTY))
                break;
        }
        /* use MBOX_READ register to check if it is a response to our message */
        if (r == *MBOX_READ)
            /* check if it is a valid successful response */
            return mbox[1] == MBOX_RESPONSE;
    }
    return 0;
}

void get_board_revision() {
    prepare_mbox_message(GET_BOARD_REVISION, 8 * 4);   // Prepare message for board revision

    if (mbox_call(MBOX_CH_PROP)) {
        uart_puts("My revision is: ");
        uart_hex(mbox[5]);
        uart_puts("\n");
    } else {
        uart_puts("Unable to query serial!\n");
    }
}
void get_ARM_memory() {
    prepare_mbox_message(GET_ARM_MEMORY, 8 * 4);   // Prepare message for ARM memory

    if (mbox_call(MBOX_CH_PROP)) {
        uart_puts("My ARM Memory's base address is: ");
        uart_hex(mbox[5]);
        uart_puts("\n");
        uart_puts("My ARM Memory's size in bytes is: ");
        uart_hex(mbox[6]);
        uart_puts("\n\n");
    } else {
        uart_puts("Unable to query serial!\n");
    }
}