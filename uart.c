#include "uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdarg.h>

void UART_init( void )
{
    UART0_BAUDDIV = 16;
    UART0_CTRL = 1;
}

static void UART_print_int( int value )
{
    char buffer[12]; 
    char *ptr = buffer + sizeof(buffer) - 1; 
    *ptr = '\0'; 

    if (value == 0) *(--ptr) = '0';
    else 
    {
        bool is_negative = (value < 0);
        if (is_negative) value = -value;
        while (value > 0) 
        {
            *(--ptr) = '0' + (value % 10);
            value /= 10;
        }
        if (is_negative) *(--ptr) = '-';
    }
    while (*ptr) UART0_DATA = (unsigned int)(*ptr++);
}

/* ADDED: String helper */
static void UART_print_string( char *string )
{
    while (*string) UART0_DATA = (unsigned int)(*string++);
}

void UART_printf( const char *s, ... )
{
    /* Critical section prevents SRT/HRT deadlocks during printing */
    taskENTER_CRITICAL();

    va_list args;
    va_start( args, s );

    while (*s != '\0') 
    {
        if (*s == '%') 
        {
            s++; 
            if (*s == 'd') 
            {
                int value = va_arg( args, int );
                UART_print_int( value );
            }
            else if (*s == 's') /* ADDED: Handle %s */
            {
                char *string = va_arg( args, char* );
                UART_print_string( string );
            }
            s++;
        } 
        else 
        {
            UART0_DATA = (unsigned int)(*s);
            s++;
        }
    }
    va_end( args );

    taskEXIT_CRITICAL();
}