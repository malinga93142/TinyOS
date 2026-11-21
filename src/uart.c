#include "../include/uart.h"
#include "../include/misc.h"
#include <stdint.h>

#define COM1 0x3F8

#define UART_DATA 0
#define UART_IER 1
#define UART_FCR 2
#define UART_LCR 3
#define UART_MCR 4
#define UART_LSR 5

void
uart_init(void)
{
  /* Disable interrupts */
  outb(COM1 + UART_IER, 0x00);
  /*
   * Enable DLAB
   * Set baud divisor = 1
   * 115200 baud */
  outb(COM1 + UART_LCR, 0x80);
  outb(COM1 + 0, 0x01);  // divisor low
  outb(COM1 + 1, 0x00);  // divisor high
  /* 8 bits, no parity, one stop bit*/
  outb(COM1 + UART_LCR, 0x03);
  /*
   * Enable FIFO
   */
  outb(COM1 + UART_FCR, 0xC7);
  /* RTS/DSR */
  outb(COM1 + UART_MCR, 0x03);
}

static int
uart_ready(void)
{
  /*
   * LSR bit 5:
   * Transmitter holding register empty
   */
  return inb(COM1 + UART_LSR) & 0x20;
}

void
uart_putc(char c)
{
  while (!uart_ready())
    ;

  outb(COM1 + UART_DATA, c);
}

void
uart_puts(const char *s)
{
  while (*s)
    {
      if (*s == '\n')
        uart_putc('\r');

      uart_putc(*s++);
    }
}
