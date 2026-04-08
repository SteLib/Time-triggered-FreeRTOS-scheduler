#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include "scheduler.h"

#define SUB_FRAME_PERIOD pdMS_TO_TICKS( 100 )

//a major frame is composed by n sub frame
SubFrame_t major_frame [NUM_SUB_FRAMES];

//--------------------- DECLARATION OF TASKS ---------------------

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
        //vTaskSuspend(NULL); 
	vTaskDelay(pdMS_TO_TICKS(100));
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
        //vTaskSuspend(NULL); 
    }
}

TimelineTaskConfig_t timeline_tasks[] = {
// name, code, type, start_time, end_time, subframe_index
    {"Task_HRT", Task_HRT_Code, HARD_RT, 0, 40,  0, NULL, 0},
    {"Task1_SRT", Task1_SRT_Code, SOFT_RT, 0, 0,  0, NULL, 0}, // start and end time for SRT are not relevant
    {"Task2_SRT", Task2_SRT_Code, SOFT_RT, 0, 20,  0, NULL, 0} // since they are executed only on IDLE time
};
// --------------------- TIMELINE CONFIG ---------------------
TimelineConfig_t system_timeline = {
    .tasks = timeline_tasks,
    .tasks_num = 3,
    .num_sub_frames = NUM_SUB_FRAMES,
    .major_frame_period = MAJOR_FRAME_DURATION,
    .sub_frame_period = SUB_FRAME_DURATION,

};

// --------------------- MAIN ---------------------
int main( void ) {
    UART_init();
    UART_printf( "System Booting...\n" );

    // Ora il puntatore non sarà più NULL
    vConfigureScheduler(&system_timeline);

    vTaskStartScheduler();

    while( 1 );
    return 0;
}
