#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include "scheduler.h"

#define SUB_FRAME_PERIOD pdMS_TO_TICKS( 100 )

// A major frame is composed by n sub frame
SubFrame_t major_frame[NUM_SUB_FRAMES];

// --------------------- HELPER FUNCTION ---------------------
/* Actively spins the CPU for a precise number of OS ticks */
static void vBurnCPU(TickType_t ticksToBurn) {
    TickType_t startTick = xTaskGetTickCount();
    while ((xTaskGetTickCount() - startTick) < ticksToBurn) {
        /* Busy wait - simulating intense calculations */
    }
}

//--------------------- DECLARATION OF TASKS ---------------------

/* Task Hard Real-Time (Will Complete Successfully) */
void Task_HRT_Normal( void *pvParameters ) {
    for(;;) {
        /* Takes 15 ticks to complete */
        vBurnCPU(15); 
        
        /* Finished safely! Go to sleep until Frame_Mgr resets us. */
        vTaskSuspend(NULL); 
    }
}

/* Task Hard Real-Time (Will Miss Deadline!) */
void Task_HRT_Overrun( void *pvParameters ) {
    for(;;) {
        /* Takes 30 ticks to complete */
        vBurnCPU(30); 
        
        /* The OS will forcefully kill it before it reaches this line */
        vTaskSuspend(NULL); 
    }
}

/* Task Soft Real-Time (Background work) */
void Task_SRT_Background( void *pvParameters ) {
    /* SRT tasks NEVER sleep. They soak up idle time in an infinite loop. */
    for(;;) {
        vBurnCPU(5);
    }
}

// --------------------- TIMELINE CONFIG ---------------------

TimelineTaskConfig_t timeline_tasks[] = {
    // 1. Starts at tick 10, deadline at tick 30. (Takes 15 ticks -> PASS)
    {"HRT_Good", Task_HRT_Normal, HARD_RT, 10, 30, 0, NULL, 0},

    // 2. Starts at tick 40, deadline at tick 50. (Takes 30 ticks -> MISS/KILL)
    {"HRT_Bad", Task_HRT_Overrun, HARD_RT, 40, 50, 0, NULL, 0}, 

    // 3. Runs during idle gaps. Start/End times are ignored.
    {"SRT_Worker", Task_SRT_Background, SOFT_RT, 0, 0, 0, NULL, 0}
};

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
    UART_printf( "System Booting: Testing Timeline Scheduler...\n" );

    vConfigureScheduler(&system_timeline);

    vTaskStartScheduler();

    while( 1 );
    return 0;
}