#include "mini_uart.h"
#include "mailbox.h"
void kernel_main()
{
	uart_init();
	uart_puts("Hello, world!\n");

	while (1) {
		uart_putc(uart_getc());
	}

    // char input_char[2];
    // uart_send_string("Hello, world!\n");

    // while(1){
    //     input_char[0] = uart_getc();
    //     input_char[1] = '\n';


    //     uart_send_string(input_char);
    // }
}