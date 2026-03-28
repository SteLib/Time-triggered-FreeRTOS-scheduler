#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include "semphr.h"
#include "timers.h"
#include "scheduler.h"

#define SUB_FRAME_PERIOD pdMS_TO_TICKS( 100 ) 

SemaphoreHandle_t semaphore;
static TimerHandle_t xAutoReloadTimer;
static TaskHandle_t Task_HRT;
static TaskHandle_t Task1_SRT;
static TaskHandle_t Task2_SRT;
BaseType_t xTimerStarted;

//a major frame is composed by n sub frame
SubFrame_t major_frame [NUM_SUB_FRAMES];

// Task Hard Real-Time (priority 2)
void Task_HRT_Code( void *pvParameters ) {
    for(;;) {
        // 1. do critical job 
        UART_printf("execution of HRT Task...\n");
        
        //...

        // 2. job done for this subframe.
        // The task suspends itself and yields the CPU to SRT tasks (Priority 1).
        vTaskSuspend(NULL); 
    }
}

// Task Soft Real-Time (Priorità 1)
void Task1_SRT_Code( void *pvParameters ) {
    for(;;) {
        // 1. do some less critical job
        UART_printf("execution of SRT Task 1...\n");
        
        //...

        // 2. job done
        // suspension of task which yields the CPU to the Idle task (Priority 0)
        vTaskSuspend(NULL); 
    }
}

// Task Soft Real-Time (Priorità 1)
void Task2_SRT_Code( void *pvParameters ) {
    for(;;) {
        // 1. do some less critical job
        UART_printf("execution of SRT Task 2...\n");
        
        //...

        // 2. job done
        // suspension of task which yields the CPU to the Idle task (Priority 0)
        vTaskSuspend(NULL); 
    }
}

static void prvAutoReloadTimerCallback(TimerHandle_t xTimer) {
   xSemaphoreGive(semaphore)  ; 
}

int main( void ) {

    UART_init();

    UART_printf( "System Booting...\n" );

    semaphore = xSemaphoreCreateBinary (); 
    
    xTaskCreate(Task_HRT_Code, "Task Hard", 128, NULL, 2, &Task_HRT);
    vTaskSuspend(Task_HRT); 
    xTaskCreate(Task1_SRT_Code, "Task Soft 1", 128, NULL, 1, &Task1_SRT); //si può provare anche con più srt
    vTaskSuspend(Task1_SRT); 
    xTaskCreate(Task2_SRT_Code, "Task Soft 2", 128, NULL, 1, &Task2_SRT); //si può provare anche con più srt
    vTaskSuspend(Task2_SRT); 
    
    // use first sub-frame as example
    major_frame[0].hrt_task = Task_HRT;
    major_frame[0].srt_tasks[0] = Task1_SRT;
    major_frame[0].srt_tasks[1] = Task2_SRT;
    major_frame[0].num_srt_tasks = 2;

    // TODO: create other sub-frame as example

    //first real executed task with highest riority to handle the sub frame
    xTaskCreate(dispatcher, "Dispatcher", 128, NULL, 3, NULL);

    xAutoReloadTimer = xTimerCreate(
        /* Text name for the software timer - not used by FreeRTOS. */
        "AutoReloadSubFrame",
        /* The software timer's period in ticks. */
        SUB_FRAME_PERIOD,
        /* Setting uxAutoRealod to pdTRUE creates an auto-reload timer. */
        pdTRUE,
         /* does not use the timer id. */
        0,
         /* Callback function to be used by the software timer being created. */
        prvAutoReloadTimerCallback 
    );
 
    /* Check the software timers were created. */
    if( xAutoReloadTimer != NULL ) {
    /* Start the software timers, using a block time of 0 (no block time).
    The scheduler has not been started yet so any block time specified
    here would be ignored anyway. */
    xTimerStarted = xTimerStart( xAutoReloadTimer, 0 );
    }
    
    vTaskStartScheduler();
    
    while( 1 );
    return 0;
}