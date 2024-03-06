#include "utils.h"
#include "command.h"
#include "mini_uart.h"
#include "mailbox.h"
#include "string.h"

void input_buffer_overflow_message ( char cmd[] )
{
    uart_puts("The following command: \"");
    uart_puts(cmd);
    uart_puts("\"... is too long to process.\n");

    uart_puts("The maximum length of input is 64.");
}


void command_hello(){
    uart_puts("Hello, World!\n");
}

void command_help(){
    uart_puts("\n");
    uart_puts("help      : print this help menu\n");
    uart_puts("hello     : print Hello World!\n");
    uart_puts("\n");
}

void command_not_found (char * s) 
{
    uart_puts("Error: command ");
    uart_puts(s);
    uart_puts(" not found, try <help>\n");
}