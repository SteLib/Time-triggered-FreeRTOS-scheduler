#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"


void vHelloWorld( void *pvParameters){
    (void)pvParameters;
    for(; ;){
        UART_printf("Hellow World\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}

int main( void )
{
    UART_init();

    UART_printf( "System Booting...\n" );

    xTaskCreate(vHelloWorld,"hello",128, NULL, 1, NULL);

    vTaskStartScheduler();
    while( 1 );
    return 0;
}