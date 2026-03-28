#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define MAJOR_FRAME_DURATION                    ( ( TickType_t ) 1000 ) //1 ms for each major frame
#define SUB_FRAME_DURATION                      ( ( TickType_t ) 100 ) //1/10 ms for each sub frame
#define NUM_SUB_FRAMES                          (MAJOR_FRAME_DURATION / SUB_FRAME_DURATION)
#define MAX_SRT_PER_SUBFRAME                        2


//sub frame for time-triggered scheduler
typedef struct {

    TaskHandle_t hrt_task; //for hard real time task, only 1 task or NULL        
 
    TaskHandle_t srt_tasks[MAX_SRT_PER_SUBFRAME]; // soft real-time task
  
    uint8_t num_srt_tasks; //numer of soft real-time task

} SubFrame_t; //_t to specify that's a type

// External reference to the major-frame array defined in main.c composed by sub-frame
extern SubFrame_t major_frame[NUM_SUB_FRAMES]; 

void dispatcher ();

void vConfigureScheduler();

#endif