/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"

/* Task classification for the time-triggered scheduler. */
typedef enum
{
    HARD_RT, /* Hard Real-Time: must finish before its deadline or is terminated. */
    SOFT_RT  /* Soft Real-Time: runs in idle gaps, preemptible by any HRT task.   */
} TaskType_t;

/* One slot inside a major frame; reserved for future sub-frame metadata. */
typedef struct
{
    uint32_t id;
} SubFrame_t;

/* Timing constants – adjust to match your target CPU / tick rate. */
#define MAJOR_FRAME_DURATION    ( ( TickType_t ) 100 )  /* ticks per major frame  */
#define SUB_FRAME_DURATION      ( ( TickType_t ) 10  )  /* ticks per sub-frame    */
#define NUM_SUB_FRAMES          ( MAJOR_FRAME_DURATION / SUB_FRAME_DURATION )
#define MAX_SRT_PER_SUBFRAME    ( 2 )

/*
 * Per-task configuration supplied by the application in main.c.
 * The scheduler reads this table; the application must not modify it
 * after vConfigureScheduler() has been called.
 */
typedef struct
{
    const char      *task_name;     /* Human-readable name (≤ configMAX_TASK_NAME_LEN). */
    TaskFunction_t   function;      /* The task entry point.                            */
    TaskType_t       type;          /* HARD_RT or SOFT_RT.                              */
    uint32_t         ulStart_time;  /* Tick offset inside the major frame to activate.  */
    uint32_t         ulEnd_time;    /* Deadline tick offset; HRT is killed if still      */
                                    /* running at this point.                            */
    uint32_t         ulSubframe_id; /* Sub-frame this task belongs to.                  */

    /* --- Runtime state (written by the scheduler, not by the application) --- */
    TaskHandle_t     xHandle;       /* FreeRTOS task handle, set by xTaskCreate().      */
    uint8_t          is_completed;  /* pdTRUE once the task has finished or been killed. */
} TimelineTaskConfig_t;

/* Global scheduler state, passed to vConfigureScheduler() once at boot. */
typedef struct
{
    TimelineTaskConfig_t *tasks;          /* Pointer to the task-config array.           */
    uint32_t              tasks_num;      /* Number of entries in that array.             */
    uint32_t              num_sub_frames; /* Derived from MAJOR_FRAME / SUB_FRAME.        */
    uint32_t              major_frame_period; /* Must equal MAJOR_FRAME_DURATION.         */
    uint32_t              sub_frame_period;   /* Must equal SUB_FRAME_DURATION.           */

    /* Dynamic counters – maintained by the tick hook, initialised to 0. */
    uint32_t              current_tick;        /* 0 … (major_frame_period - 1) */
    uint32_t              current_sub_frame;   /* 0 … (num_sub_frames - 1)     */
} TimelineConfig_t;

/* External reference to the sub-frame array defined in main.c. */
extern SubFrame_t major_frame[ NUM_SUB_FRAMES ];

/*
 * vConfigureScheduler()
 *
 * Call once from main(), before vTaskStartScheduler().
 * Parses cfg, creates all FreeRTOS tasks (suspended), and spawns the
 * Frame Manager task that resets the timeline at every major-frame boundary.
 */
void vConfigureScheduler( TimelineConfig_t *cfg );

/*
 * vTaskSetTimelineConfig()
 *
 * Writes the timeline parameters directly into the TCB extension fields
 * added by the tasks.c patch.  Call after xTaskCreate() for each task
 * whose start/end times must be enforced inside xTaskIncrementTick().
 */
void vTaskSetTimelineConfig( TaskHandle_t xTask,
                             uint32_t ulStart,
                             uint32_t ulEnd,
                             uint8_t type );

#endif /* SCHEDULER_H */