#ifndef __PRINTF__
#define __PRINTF__

#include "FreeRTOS.h"

#include <stdbool.h>
#include <stdarg.h> // For va_list, va_start, va_end


#define UART0_ADDRESS                         ( 0x40004000UL )
#define UART0_DATA                            ( *( ( ( volatile uint32_t * ) ( UART0_ADDRESS + 0UL ) ) ) )
#define UART0_STATE                           ( *( ( ( volatile uint32_t * ) ( UART0_ADDRESS + 4UL ) ) ) )
#define UART0_CTRL                            ( *( ( ( volatile uint32_t * ) ( UART0_ADDRESS + 8UL ) ) ) )
#define UART0_BAUDDIV                         ( *( ( ( volatile uint32_t * ) ( UART0_ADDRESS + 16UL ) ) ) )


void UART_init(void);
// void UART_printf(const char *s);
void UART_printf(const char *s, ...);

void UART_printf_ISR(const char *s, ...);

#endif


