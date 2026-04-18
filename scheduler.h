#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"

// --- 1. AGGIUNTO: Definizione TaskType_t (Mancava e bloccava tutto) ---
typedef enum {
    HARD_RT,
    SOFT_RT
} TaskType_t;

// --- 2. AGGIUNTO: Definizione minima di SubFrame_t (Mancava) ---
typedef struct {
    uint32_t id;
} SubFrame_t;

#define MAJOR_FRAME_DURATION                    ( ( TickType_t ) 1000 ) //1 ms for each major frame
#define SUB_FRAME_DURATION                      ( ( TickType_t ) 100 ) //1/10 ms for each sub frame
#define NUM_SUB_FRAMES                          (MAJOR_FRAME_DURATION / SUB_FRAME_DURATION)
#define MAX_SRT_PER_SUBFRAME                        2

// given by the Professor but need some changes (last two lines)
typedef struct {
    const char* task_name;
    TaskFunction_t function;
    TaskType_t type; // HARD_RT or SOFT_RT
    uint32_t ulStart_time;
    uint32_t ulEnd_time;
    uint32_t ulSubframe_id;

    TaskHandle_t xHandle; // handle genrated by xTaskCreate when the task is created
    uint8_t is_completed; // flag to indicate if the task has completed its execution in the current subframe
} TimelineTaskConfig_t;

// system's global state
typedef struct{
        TimelineTaskConfig_t* tasks;
        uint32_t tasks_num;
	    uint32_t num_sub_frames; //mancava!
        uint32_t major_frame_period;
        uint32_t sub_frame_period;

        // --- Stato Dinamico del Sistema (I contatori mancanti) ---
        uint32_t current_tick;        // Va da 0 a (major_frame_period - 1)
        uint32_t current_sub_frame;   // Va da 0 a (num_sub_frames - 1)
} TimelineConfig_t;

// external reference to the major-frame array defined in main.c composed by sub-frame
extern SubFrame_t major_frame[NUM_SUB_FRAMES]; 

void vConfigureScheduler(TimelineConfig_t* cfg); // inizialization of the timeline scheduler

#endif
void vTaskSetTimelineConfig(TaskHandle_t xTask, uint32_t ulStart, uint32_t ulEnd, uint8_t type);
