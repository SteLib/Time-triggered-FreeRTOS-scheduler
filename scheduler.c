#include "scheduler.h"
# include "uart.h"

extern SubFrame_t major_frame[NUM_SUB_FRAMES];

// this pointer will be read by tasks.c
// it contains the configuration of the timeline and the tasks to be executed, 
//and it is set by vConfigureScheduler() when the user calls it in main.c
TimelineConfig_t* pxTimelineState = NULL;


// =========================================================================
// CONFIGURATION OF THE SCHEDULER
// =========================================================================
void vConfigureScheduler(TimelineConfig_t *cfg) {
    if (cfg == NULL || cfg->tasks == NULL || cfg->task_count == 0) {
        UART_printf("Error: Timeline configuration not valid!\n");
        return;
    }

    /// save the config into the pointer
    pxTimelineState = cfg;

    UART_printf("Initialization of Time-Triggered Scheduler\n");
    UART_printf("Task totali configurati: %d\n", cfg->task_count);

    // we're creating the task and suspend them immeditaly
    for (uint32_t i = 0; i < cfg->task_count; i++) {
        TimelineTaskConfig_t* t = &cfg->tasks[i];

        // state not completed for the current subframe
        t->is_completed = 0;
        t->xHandle = NULL;

        // ------------------------- CREATION OF THE TASKS ---------------------------- 
        // we set priority to 1 for all task since the scheduler is timeline-based
        BaseType_t xReturned = xTaskCreate(
            t->function,
            t->task_name,
            configMINIMAL_STACK_SIZE * 2, // Aumenta se i tuoi task fanno calcoli pesanti
            NULL,
            1, // Priorità fittizia
            &(t->xHandle)
        );

        if (xReturned == pdPASS) {
            vTaskSuspend(t->xHandle); // because is kernel's work
            
            UART_printf("[+] Task created and suspended: %s\n", t->task_name);
        } else {
            UART_printf("[-] Error creating task: %s\n", t->task_name);
        }
    }
}

  