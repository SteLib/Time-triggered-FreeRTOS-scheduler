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

static void UART_print_hex( unsigned int value )
{
    char hex_chars[] = "0123456789ABCDEF";
    UART_print_string("0x");
    for (int i = 28; i >= 0; i -= 4)
    {
        UART0_DATA = hex_chars[(value >> i) & 0xF];
    }
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
	    if (*s == 'd' || *s == 'i')
            {
                int value = va_arg( args, int );
                UART_print_int( value );
            }
            else if (*s == 'u')
            {
                unsigned int value = va_arg( args, unsigned int );
                UART_print_int( (int)value ); // Riutilizziamo print_int per semplicità
            }
            else if (*s == 's')
            {
                char *string = va_arg( args, char* );
                if (string == NULL) UART_print_string("(null)");
                else UART_print_string( string );
            }
            else if (*s == 'p' || *s == 'x')
            {
                unsigned int value = (unsigned int)va_arg( args, void* );
                UART_print_hex( value );
            }
            s++; // Passa al carattere dopo la specifica (es. dopo 'd')
        }
        else
        {
            UART0_DATA = (unsigned int)(*s);
            s++;
        }
    }

    va_end( args );

    /* IMPORTANTE: Devi uscire dalla sezione critica o il sistema si blocca! */
    taskEXIT_CRITICAL();
}
