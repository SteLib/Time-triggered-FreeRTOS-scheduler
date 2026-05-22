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

#include "scheduler.h"
#include "uart.h"

/* Forward declaration of the Frame Manager task body. */
static void vFrameManagerTask( void *pvParameters );

/* -------------------------------------------------------------------------
 * Module-level globals
 * ---------------------------------------------------------------------- */

/*
 * pxTimelineState
 *
 * Set once by vConfigureScheduler() and then read (never written) by the
 * tick-hook logic inside tasks.c.  Declared extern there so the linker
 * connects both translation units.
 */
TimelineConfig_t *pxTimelineState = NULL;

/*
 * xFrameManagerHandle
 *
 * Handle of the Frame Manager task.  The tick hook resumes it at every
 * major-frame boundary (now == 0) so it can reset HRT tasks.
 * Also declared extern in tasks.c.
 */
TaskHandle_t xFrameManagerHandle = NULL;

/* -------------------------------------------------------------------------
 * Helper: (re)create one HRT task and leave it suspended
 * ---------------------------------------------------------------------- */
static BaseType_t prvRecreateHRTTask( TimelineTaskConfig_t *t )
{
    BaseType_t xResult;

    /* Delete the old instance if it still exists. */
    if( t->xHandle != NULL )
    {
        vTaskDelete( t->xHandle );
        t->xHandle = NULL;
    }

    /* Create a fresh instance at HRT priority. */
    xResult = xTaskCreate(
        t->function,
        t->task_name,
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        2,              /* Priority 2 – HRT level */
        &( t->xHandle )
    );

    if( xResult == pdPASS )
    {
        /* Leave it suspended; the tick hook will resume it at ulStart_time. */
        vTaskSuspend( t->xHandle );
        t->is_completed = 0;
    }

    return xResult;
}

/* -------------------------------------------------------------------------
 * vConfigureScheduler()
 * ---------------------------------------------------------------------- */
void vConfigureScheduler( TimelineConfig_t *cfg )
{
    uint32_t i;
    BaseType_t xReturned;

    /* --- Validate input ------------------------------------------------- */
    if( ( cfg == NULL ) || ( cfg->tasks == NULL ) || ( cfg->tasks_num == 0U ) )
    {
        UART_printf( "Error: Timeline configuration not valid!\n" );
        return;
    }

    /* --- CLEANUP PREVIOUS TEST TO PREVENT MEMORY CRASHES --- */
    extern TaskHandle_t xFrameManagerHandle;
    extern TimelineConfig_t *pxTimelineState;
    
    if (xFrameManagerHandle != NULL) {
        vTaskDelete(xFrameManagerHandle);
        xFrameManagerHandle = NULL;
    }
    if (pxTimelineState != NULL) {
        for (uint32_t i = 0; i < pxTimelineState->tasks_num; i++) {
            if (pxTimelineState->tasks[i].xHandle != NULL) {
                vTaskDelete(pxTimelineState->tasks[i].xHandle);
                pxTimelineState->tasks[i].xHandle = NULL;
            }
        }
    }
    /* ------------------------------------------------------- */

    /* Save the config pointer so tasks.c can reach it. */
    pxTimelineState = cfg;

    UART_printf( "Initialization of Time-Triggered Scheduler\n" );
    UART_printf( "Total task count: %d\n", cfg->tasks_num );

    /* --- Spawn the Frame Manager ---------------------------------------- */
    /*
     * Priority 3 (highest) so it immediately preempts HRT (2) and SRT (1)
     * when the tick hook resumes it at the start of every major frame.
     */
    xReturned = xTaskCreate(
        vFrameManagerTask,
        "Frame_Mgr",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        3,
        &xFrameManagerHandle
    );

    if( xReturned == pdPASS )
    {
        UART_printf( "[+] Frame Manager Task created successfully\n" );
    }
    else
    {
        UART_printf( "[-] Error creating Frame Manager Task!\n" );
    }

    /* --- Create application tasks, all initially suspended -------------- */
    for( i = 0; i < cfg->tasks_num; i++ )
    {
        TimelineTaskConfig_t *t = &cfg->tasks[ i ];

        t->is_completed = 0;
        t->xHandle      = NULL;

        /* HRT runs at priority 2; SRT at priority 1.
         * Because configUSE_TIME_SLICING == 0 and the tick hook enforces
         * the window boundaries, priority alone is sufficient – we do NOT
         * need to override pxCurrentTCB manually in vTaskSwitchContext. */
        UBaseType_t uxPriority = ( t->type == HARD_RT ) ? 2U : 1U;

        xReturned = xTaskCreate(
            t->function,
            t->task_name,
            configMINIMAL_STACK_SIZE * 2,
            NULL,
            uxPriority,
            &( t->xHandle )
        );

        if( xReturned == pdPASS )
        {
            vTaskSuspend( t->xHandle );
            UART_printf( "[+] Task created and suspended: %s\n", t->task_name );
        }
        else
        {
            UART_printf( "[-] Error creating task: %s\n", t->task_name );
        }
    }
}

/* -------------------------------------------------------------------------
 * vFrameManagerTask()
 *
 * Runs at the start of every major frame (resumed by the tick hook when
 * now == 0).  Rebuilds every HRT task so each frame starts from a clean
 * state, regardless of whether the previous instance finished or was killed.
 *
 * FIX: the original version called vTaskSuspend(NULL) at the very top,
 *      which meant it suspended itself before doing any work on the first
 *      activation, AND was never resumed again because the tick hook only
 *      woke SRT tasks at now==0.  The corrected version:
 *        1. Does its reset work FIRST.
 *        2. Then suspends itself to wait for the NEXT major-frame boundary.
 *      The tick hook resumes it at now==0 each cycle.
 * ---------------------------------------------------------------------- */
static void vFrameManagerTask( void *pvParameters )
{
    uint32_t i;

    ( void ) pvParameters;

    for( ;; )
    {
        /* --- Reset all HRT tasks for the new major frame ---------------- */
        if( pxTimelineState != NULL )
        {
            for( i = 0; i < pxTimelineState->tasks_num; i++ )
            {
                TimelineTaskConfig_t *t = &pxTimelineState->tasks[ i ];

                if( t->type == HARD_RT )
                {
                    if( prvRecreateHRTTask( t ) != pdPASS )
                    {
                        UART_printf( "[-] Frame_Mgr: failed to recreate %s\n",
                                     t->task_name );
                    }
                }
            }
        }

        UART_printf_ISR( "--- [ %d ms ] MAJOR FRAME RESET COMPLETE ---\n",
                         ( int ) xTaskGetTickCount() );

        /* TELL THE TEST SUITE THE FRAME IS DONE (EVT_FRAME_RESET = 3) */
        extern void vTestLogEvent(int eType, const char *pcName, uint32_t xTick, uint32_t ulFrameTick);
        vTestLogEvent(3, "Frame_Mgr", xTaskGetTickCount(), 0);

        /*
         * Work is done – suspend ourselves.
         */
        vTaskSuspend( NULL );
    }
}


/* -------------------------------------------------------------------------
 * vApplicationTickHook()
 *
 * Called from xTaskIncrementTick() on every tick when
 * configUSE_TICK_HOOK == 1.
 *
 * NOTE: The primary scheduling logic (wake/kill HRT, wake Frame Manager)
 *       lives in the tasks.c patch inside xTaskIncrementTick() so that it
 *       has direct access to kernel internals (xSwitchRequired, list ops).
 *       This hook is intentionally empty; it exists only to satisfy the
 *       linker requirement when configUSE_TICK_HOOK == 1.
 * ---------------------------------------------------------------------- */
void vApplicationTickHook( void )
{
    /* Intentionally empty – see tasks.c patch. */
}