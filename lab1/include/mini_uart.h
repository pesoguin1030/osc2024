#ifndef	_MINI_UART_H
#define	_MINI_UART_H

void uart_init ();
char uart_getc ();
void uart_putc (char c);
void uart_puts (char* str);

#endif  /*_MINI_UART_H */