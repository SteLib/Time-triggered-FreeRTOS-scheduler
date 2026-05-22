#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include "test_scheduler.h"

int main( void ) {
    UART_init();
    
    /* This function creates the supervisor task that sequences all 8 tests */
    vRunAllSchedulerTests();   
    
    /* Start the scheduler. The supervisor task takes over from here. */
    vTaskStartScheduler();
    
    while( 1 );
    return 0;
}