#include "mini_uart.h"
#include "shell.h"
#include "mailbox.h"

void kernel_main()
{
	// print board revision and ARM memory
    get_board_revision();
    get_ARM_memory();
    
    // set up serial console
    uart_init();
    
    // say hello
    uart_puts("Hello World!\n");

    // start shell
    shell_start();

}