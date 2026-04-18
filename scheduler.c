#include "scheduler.h"
#include "uart.h"

extern SubFrame_t major_frame[NUM_SUB_FRAMES];

// this pointer will be read by tasks.c
// it contains the configuration of the timeline and the tasks to be executed, 
//and it is set by vConfigureScheduler() when the user calls it in main.c
TimelineConfig_t* pxTimelineState = NULL;


// =========================================================================
// CONFIGURATION OF THE SCHEDULER
// =========================================================================
void vConfigureScheduler(TimelineConfig_t *cfg) {
    if (cfg == NULL || cfg->tasks == NULL || cfg->tasks_num == 0) {
        UART_printf("Error: Timeline configuration not valid!\n", cfg);
        return;
    }

    /// save the config into the pointer
    pxTimelineState = cfg;

    UART_printf("Initialization of Time-Triggered Scheduler\n");
    UART_printf("Total task count: %d\n", cfg->tasks_num);

    // we're creating the task and suspend them immeditaly
    for (uint32_t i = 0; i < cfg->tasks_num; i++) {
        TimelineTaskConfig_t* t = &cfg->tasks[i];

        // state not completed for the current subframe
        t->is_completed = 0;
        t->xHandle = NULL;

        // HRT = priority 2, SRT = priority 1
        UBaseType_t priority = (t->type == HARD_RT) ? 2 : 1;

        // ------------------------- CREATION OF THE TASKS ---------------------------- 
        // we set priority to 1 for all task since the scheduler is timeline-based
        BaseType_t xReturned = xTaskCreate(
            t->function,
            t->task_name,
            configMINIMAL_STACK_SIZE * 2, // Aumenta se i tuoi task fanno calcoli pesanti
            NULL,
            priority,
            &(t->xHandle)
        );

        if (xReturned == pdPASS) {
            vTaskSuspend(t->xHandle); // because is kernel's work
            
            // // Set the custom timeline parameters into the TCB!
            // vTaskSetTimelineConfig(t->xHandle, t->ulStart_time, t->ulEnd_time, t->type);
            
            UART_printf("[+] Task created and suspended: %s\n", t->task_name);
        } else {
            UART_printf("[-] Error creating task: %s\n", t->task_name);
        }
    }
}

// void vApplicationTickHook(void) {
//     static uint32_t global_tick_count = 0;
//     uint32_t now = global_tick_count % pxTimelineState->major_frame_period;

//     for (uint32_t i = 0; i < pxTimelineState->tasks_num; i++) {
//         TimelineTaskConfig_t* t = &pxTimelineState->tasks[i];

//         if (t->type == HARD_RT) {
//             // Risveglia l'HRT SOLO al suo tempo di inizio
//             if (now == t->ulStart_time) {
//                 t->is_completed = 0;
//                 xTaskResumeFromISR(t->xHandle);
//             }
//             // Sospende l'HRT SOLO alla deadline (Enforcement)
// 	    else if (now == t->ulEnd_time) {
//                 t->is_completed = 1;
//                 vTaskSuspend(t->xHandle);
//             }
//         }
//         else if (t->type == SOFT_RT) {
//             // Gli SRT devono essere sempre pronti (Ready)
//             // Se l'HRT è sospeso o non è ancora l'ora, gireranno loro.
//             xTaskResumeFromISR(t->xHandle);
//         }
//     }

//     // Ferma il test dopo 2 cicli (opzionale, per debug)
//     if (global_tick_count >= (pxTimelineState->major_frame_period * 2)) {
//         UART_printf("\n--- STOP TEST ---\n");
//         vTaskSuspendAll();
//         while(1);
//     }

//     global_tick_count++;
//     portYIELD_FROM_ISR(pdTRUE);
// }

void vApplicationTickHook(void) {
    // it does nothing
}