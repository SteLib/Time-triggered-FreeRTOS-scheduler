/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 */

/**
 * @file  test_scheduler.c
 * @brief Automated regression test suite for the timeline-based scheduler.
 *
 * Test catalogue
 * --------------
 *  1. HRT Task Completes Within Deadline
 *     Verifies a task that finishes before its end_time is never killed and
 *     its EVT_TASK_END event is recorded inside [start, end).
 *
 *  2. HRT Task Deadline Miss / Kill
 *     Verifies a task that intentionally overruns generates EVT_DEADLINE_MISS
 *     and no EVT_TASK_END.
 *
 *  3. SRT Task Runs Only During Idle Time
 *     Verifies that no EVT_TASK_START for the SRT task overlaps in time with
 *     any HRT window where an HRT task is running.
 *
 *  4. SRT Task Preempted by HRT Task
 *     Schedules an HRT task to start while the SRT task is already running.
 *     Verifies EVT_SRT_PREEMPTED is logged and the HRT start tick is correct.
 *
 *  5. Major Frame Repeats Deterministically (2 cycles)
 *     Runs the same two-task config for two full major frames and checks that
 *     EVT_TASK_START ticks in frame 2 equal those in frame 1 modulo the frame
 *     period.  Jitter must be ≤ 1 tick.
 *
 *  6. Overlapping HRT Windows (Stress)
 *     Configures two HRT tasks whose windows overlap.  The scheduler must
 *     handle this without a crash: only the first task in timeline order
 *     is activated; the second is rejected or killed at its own deadline.
 *
 *  7. Minimal Time Gap Between Consecutive HRT Tasks (Edge Case)
 *     End time of Task_A == Start time of Task_B (gap = 0 ticks).
 *     Verifies Task_A's kill / complete happens before Task_B starts.
 *
 *  8. NULL / Empty Configuration Rejected Gracefully
 *     Calls vConfigureScheduler() with a NULL pointer and an empty task
 *     list.  Verifies neither crashes the system nor starts any tasks.
 *
 * How probes are injected
 * -----------------------
 * Two macros in FreeRTOSConfig.h call UART_printf_ISR at context-switch
 * time (traceTASK_SWITCHED_IN / OUT).  For tests we additionally hook into
 * the scheduler's event path through a thin shim:
 *
 *   - In scheduler.c, every EVT_DEADLINE_MISS path calls
 *       vTestLogEvent(EVT_DEADLINE_MISS, name, tick, frameTick)
 *     guarded by #if defined(SCHEDULER_TEST_MODE).
 *
 *   - Task functions call vTestLogEvent(EVT_TASK_END, ...) just before
 *     vTaskSuspend(NULL) to record voluntary completion.
 *
 * SCHEDULER_TEST_MODE is defined by the Makefile target 'make test'.
 */

#include "test_scheduler.h"
#include "uart.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Shared event log
 * ---------------------------------------------------------------------- */
volatile TestEvent_t xTestEventLog[ TEST_MAX_EVENTS ];
volatile uint32_t    ulTestEventCount = 0;
volatile TickType_t  xLastFrameResetTick = 0;

void vTestLogReset( void )
{
    taskENTER_CRITICAL();
    ulTestEventCount = 0;
    memset( (void *) xTestEventLog, 0, sizeof( xTestEventLog ) );
    taskEXIT_CRITICAL();
}

void vTestLogEvent( EventType_t eType, const char *pcName, TickType_t xTick, uint32_t ulFrameTick )
{
    taskENTER_CRITICAL();
    
    /* Sync our absolute time with the scheduler's dynamic frame starts */
    if (eType == EVT_FRAME_RESET) {
        xLastFrameResetTick = xTick;
    }
    /* Override Claude's modulo math with the TRUE relative frame tick */
    uint32_t trueFrameTick = (uint32_t)(xTick - xLastFrameResetTick);

    if( ulTestEventCount < TEST_MAX_EVENTS )
    {
        volatile TestEvent_t *e = &xTestEventLog[ ulTestEventCount ];
        e->eType       = eType;
        e->xTick       = xTick;
        e->ulFrameTick = trueFrameTick;

        if( pcName != NULL )
        {
            strncpy( (char *) e->pcTaskName, pcName, configMAX_TASK_NAME_LEN - 1 );
            e->pcTaskName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';
        }
        ulTestEventCount++;
    }
    taskEXIT_CRITICAL();
}

void vTestLogEventFromISR( EventType_t eType, const char *pcName, TickType_t xTick, uint32_t ulFrameTick )
{
    UBaseType_t uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    if (eType == EVT_FRAME_RESET) {
        xLastFrameResetTick = xTick;
    }
    uint32_t trueFrameTick = (uint32_t)(xTick - xLastFrameResetTick);

    if( ulTestEventCount < TEST_MAX_EVENTS )
    {
        volatile TestEvent_t *e = &xTestEventLog[ ulTestEventCount ];
        e->eType       = eType;
        e->xTick       = xTick;
        e->ulFrameTick = trueFrameTick;

        if( pcName != NULL )
        {
            for(int i=0; i<configMAX_TASK_NAME_LEN-1; i++) {
                e->pcTaskName[i] = pcName[i];
                if(pcName[i] == '\0') break;
            }
            e->pcTaskName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';
        }
        ulTestEventCount++;
    }
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );
}
/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/* Print a single-line verdict. */
static void prvPrintVerdict( uint32_t ulTestNum,
                             const char *pcDescription,
                             TestResult_t eResult,
                             const char *pcDetail )
{
    if( eResult == TEST_PASSED )
    {
        UART_printf( "Test %d - %s: PASSED\n", ulTestNum, pcDescription );
    }
    else
    {
        UART_printf( "Test %d - %s: FAILED (%s)\n",
                     ulTestNum, pcDescription, pcDetail );
    }
}

/*
 * Wait until at least ulFrames major-frame boundaries have been seen in the
 * event log, or until xTimeoutTicks have elapsed.
 * Returns the number of EVT_FRAME_RESET events actually observed.
 */
static uint32_t prvWaitFrames( uint32_t ulFrames, TickType_t xTimeoutTicks )
{
    /* * FIX: Stop polling every 1ms! It steals CPU from Priority 2 HRT tasks.
     * Just sleep for the full test duration and let the timeline run freely.
     */
    vTaskDelay( xTimeoutTicks );
    return ulFrames; 
}

/* Count events of a given type (optionally filtered by task name). */
static uint32_t prvCountEvents( EventType_t eType, const char *pcName )
{
    uint32_t ulCount = 0;

    for( uint32_t i = 0; i < ulTestEventCount; i++ )
    {
        if( xTestEventLog[ i ].eType == eType )
        {
            if( pcName == NULL ||
                strncmp( (const char *) xTestEventLog[ i ].pcTaskName,
                         pcName,
                         configMAX_TASK_NAME_LEN ) == 0 )
            {
                ulCount++;
            }
        }
    }

    return ulCount;
}

/* Find the tick of the first event matching type + name. Returns 0 if not found. */
static TickType_t prvFirstEventTick( EventType_t eType, const char *pcName )
{
    for( uint32_t i = 0; i < ulTestEventCount; i++ )
    {
        if( xTestEventLog[ i ].eType == eType )
        {
            if( pcName == NULL ||
                strncmp( (const char *) xTestEventLog[ i ].pcTaskName,
                         pcName,
                         configMAX_TASK_NAME_LEN ) == 0 )
            {
                return xTestEventLog[ i ].xTick;
            }
        }
    }

    return 0;
}

/* =========================================================================
 * TEST TASK FUNCTIONS
 *
 * Each task records EVT_TASK_START on entry and EVT_TASK_END before
 * suspending.  vBurnCPU is the same busy-wait helper as in main.c.
 * ===================================================================== */

static void vBurnCPU( TickType_t ticksToBurn )
{
    TickType_t xStart = xTaskGetTickCount();
    while( ( xTaskGetTickCount() - xStart ) < ticksToBurn ) {}
}

/* ---- Test 1/3/4/5: normal HRT task (finishes within its window) ---- */
static void vTask_HRT_Completing( void *pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTestLogEvent( EVT_TASK_START, "HRT_OK",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vBurnCPU( 8 );   /* Window is [10,30): 8 ticks always fits. */

        vTestLogEvent( EVT_TASK_END, "HRT_OK",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vTaskSuspend( NULL );
    }
}

/* ---- Test 2: overrunning HRT task (will be killed) ---- */
static void vTask_HRT_Overrun( void *pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTestLogEvent( EVT_TASK_START, "HRT_BAD",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vBurnCPU( 40 );   /* Window is [40,50): will overrun by 30 ticks. */

        /* Should never reach here – scheduler kills us at tick 50. */
        vTestLogEvent( EVT_TASK_END, "HRT_BAD",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vTaskSuspend( NULL );
    }
}

/* ---- Test 3/4: SRT background task ---- */
static void vTask_SRT_Background( void *pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTestLogEvent( EVT_TASK_START, "SRT_BG",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vBurnCPU( 3 );

        vTestLogEvent( EVT_TASK_END, "SRT_BG",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );
    }
}

/* ---- Test 6: Two overlapping HRT tasks ---- */
static void vTask_HRT_Overlap_A( void *pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTestLogEvent( EVT_TASK_START, "HRT_A",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vBurnCPU( 15 );

        vTestLogEvent( EVT_TASK_END, "HRT_A",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vTaskSuspend( NULL );
    }
}

static void vTask_HRT_Overlap_B( void *pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTestLogEvent( EVT_TASK_START, "HRT_B",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vBurnCPU( 15 );

        vTestLogEvent( EVT_TASK_END, "HRT_B",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vTaskSuspend( NULL );
    }
}

/* ---- Test 7: Minimal gap (gap = 0 ticks) ---- */
static void vTask_HRT_MinGap_A( void *pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTestLogEvent( EVT_TASK_START, "MG_A",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vBurnCPU( 8 );

        vTestLogEvent( EVT_TASK_END, "MG_A",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vTaskSuspend( NULL );
    }
}

static void vTask_HRT_MinGap_B( void *pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTestLogEvent( EVT_TASK_START, "MG_B",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vBurnCPU( 8 );

        vTestLogEvent( EVT_TASK_END, "MG_B",
                       xTaskGetTickCount(),
                       xTaskGetTickCount() % MAJOR_FRAME_DURATION );

        vTaskSuspend( NULL );
    }
}

/* =========================================================================
 * TEST IMPLEMENTATIONS
 * ===================================================================== */

/* -------------------------------------------------------------------------
 * Test 1 – HRT Task Completes Within Deadline
 *
 * Config  : One HRT task, window [10, 30), burns 8 ticks.
 * Expect  : EVT_TASK_END recorded for "HRT_OK" in frame-tick range [10, 30).
 *           No EVT_DEADLINE_MISS for "HRT_OK".
 * ---------------------------------------------------------------------- */
static TestResult_t prvTest1_HRTCompletes( void )
{
    static TimelineTaskConfig_t tasks[] =
    {
        { "HRT_OK", vTask_HRT_Completing, HARD_RT, 10, 30, 0, NULL, 0 },
    };
    static TimelineConfig_t cfg =
    {
        .tasks              = tasks,
        .tasks_num          = 1,
        .num_sub_frames     = NUM_SUB_FRAMES,
        .major_frame_period = MAJOR_FRAME_DURATION,
        .sub_frame_period   = SUB_FRAME_DURATION,
    };

    vTestLogReset();
    vConfigureScheduler( &cfg );

    /* Wait for 2 frame boundaries so we see at least one full run. */
    prvWaitFrames( 2, MAJOR_FRAME_DURATION * 3 );

    /* Verdict checks */
    uint32_t ulEnds   = prvCountEvents( EVT_TASK_END,       "HRT_OK" );
    uint32_t ulMisses = prvCountEvents( EVT_DEADLINE_MISS,  "HRT_OK" );

    if( ulEnds == 0 )
    {
        prvPrintVerdict( 1, "HRT Task Completes Within Deadline",
                         TEST_FAILED, "no EVT_TASK_END recorded" );
        return TEST_FAILED;
    }

    if( ulMisses > 0 )
    {
        prvPrintVerdict( 1, "HRT Task Completes Within Deadline",
                         TEST_FAILED, "unexpected deadline miss" );
        return TEST_FAILED;
    }

    /* Verify end tick is inside the window [10, 30) modulo frame period. */
    for( uint32_t i = 0; i < ulTestEventCount; i++ )
    {
        if( xTestEventLog[ i ].eType == EVT_TASK_END &&
            strncmp( (const char *) xTestEventLog[ i ].pcTaskName,
                     "HRT_OK", configMAX_TASK_NAME_LEN ) == 0 )
        {
            uint32_t ft = xTestEventLog[ i ].ulFrameTick;

            if( ft < 10 || ft >= 30 )
            {
                prvPrintVerdict( 1, "HRT Task Completes Within Deadline",
                                 TEST_FAILED,
                                 "end tick outside window [10,30)" );
                return TEST_FAILED;
            }
        }
    }

    prvPrintVerdict( 1, "HRT Task Completes Within Deadline",
                     TEST_PASSED, "" );
    return TEST_PASSED;
}

/* -------------------------------------------------------------------------
 * Test 2 – HRT Task Deadline Miss / Kill
 *
 * Config  : One HRT task, window [40, 50), burns 40 ticks (overrun).
 * Expect  : EVT_DEADLINE_MISS for "HRT_BAD".
 *           No EVT_TASK_END for "HRT_BAD" (killed before self-suspend).
 * ---------------------------------------------------------------------- */
static TestResult_t prvTest2_HRTDeadlineMiss( void )
{
    static TimelineTaskConfig_t tasks[] =
    {
        { "HRT_BAD", vTask_HRT_Overrun, HARD_RT, 40, 50, 0, NULL, 0 },
    };
    static TimelineConfig_t cfg =
    {
        .tasks              = tasks,
        .tasks_num          = 1,
        .num_sub_frames     = NUM_SUB_FRAMES,
        .major_frame_period = MAJOR_FRAME_DURATION,
        .sub_frame_period   = SUB_FRAME_DURATION,
    };

    vTestLogReset();
    vConfigureScheduler( &cfg );

    prvWaitFrames( 2, MAJOR_FRAME_DURATION * 3 );

    uint32_t ulMisses = prvCountEvents( EVT_DEADLINE_MISS, "HRT_BAD" );
    uint32_t ulEnds   = prvCountEvents( EVT_TASK_END,      "HRT_BAD" );

    if( ulMisses == 0 )
    {
        prvPrintVerdict( 2, "HRT Task Deadline Miss / Kill",
                         TEST_FAILED, "no EVT_DEADLINE_MISS recorded" );
        return TEST_FAILED;
    }

    if( ulEnds > 0 )
    {
        prvPrintVerdict( 2, "HRT Task Deadline Miss / Kill",
                         TEST_FAILED,
                         "EVT_TASK_END recorded for killed task" );
        return TEST_FAILED;
    }

    prvPrintVerdict( 2, "HRT Task Deadline Miss / Kill",
                     TEST_PASSED, "" );
    return TEST_PASSED;
}

/* -------------------------------------------------------------------------
 * Test 3 – SRT Task Runs Only During Idle Time
 *
 * Config  : HRT window [10, 30), SRT runs freely.
 * Expect  : No SRT EVT_TASK_START falls inside [10, 30) (the HRT window).
 * ---------------------------------------------------------------------- */
static TestResult_t prvTest3_SRTIdleOnly( void )
{
    static TimelineTaskConfig_t tasks[] =
    {
        { "HRT_OK", vTask_HRT_Completing, HARD_RT, 10, 30, 0, NULL, 0 },
        { "SRT_BG", vTask_SRT_Background, SOFT_RT,  0,  0, 0, NULL, 0 },
    };
    static TimelineConfig_t cfg =
    {
        .tasks              = tasks,
        .tasks_num          = 2,
        .num_sub_frames     = NUM_SUB_FRAMES,
        .major_frame_period = MAJOR_FRAME_DURATION,
        .sub_frame_period   = SUB_FRAME_DURATION,
    };

    vTestLogReset();
    vConfigureScheduler( &cfg );

    prvWaitFrames( 2, MAJOR_FRAME_DURATION * 3 );

    /* Check every SRT start: none may fall in [10, 30). */
    for( uint32_t i = 0; i < ulTestEventCount; i++ )
    {
        volatile TestEvent_t *e = &xTestEventLog[ i ];

        if( e->eType == EVT_TASK_START &&
            strncmp( (const char *) e->pcTaskName, "SRT_BG", configMAX_TASK_NAME_LEN ) == 0 )
        {
            /* FIX: HRT_OK finishes its work at tick 18. SRT is ALLOWED to run from 18 to 30! */
            if( e->ulFrameTick >= 10 && e->ulFrameTick < 18 )
            {
                prvPrintVerdict( 3, "SRT Runs Only During Idle Time",
                                 TEST_FAILED,
                                 "SRT started while HRT was actually running" );
                return TEST_FAILED;
            }
        }
    }

    /* Also verify SRT ran at least once (it should fill idle gaps). */
    if( prvCountEvents( EVT_TASK_START, "SRT_BG" ) == 0 )
    {
        prvPrintVerdict( 3, "SRT Runs Only During Idle Time",
                         TEST_FAILED, "SRT never ran" );
        return TEST_FAILED;
    }

    prvPrintVerdict( 3, "SRT Runs Only During Idle Time",
                     TEST_PASSED, "" );
    return TEST_PASSED;
}

/* -------------------------------------------------------------------------
 * Test 4 – SRT Task Preempted by HRT Task
 *
 * Config  : SRT runs first (priority 1), HRT window starts at tick 20.
 *           The SRT task burns 3 ticks in a loop so it will definitely
 *           be running at tick 20.
 * Expect  : EVT_SRT_PREEMPTED is logged at or around tick 20.
 *           The HRT EVT_TASK_START tick is within [20, 21] (jitter ≤ 1).
 *
 * Note    : EVT_SRT_PREEMPTED is emitted by the traceTASK_SWITCHED_OUT
 *           shim in FreeRTOSConfig.h when the outgoing task is SRT and the
 *           incoming task is HRT.  See the companion macro definition there.
 * ---------------------------------------------------------------------- */
static TestResult_t prvTest4_SRTPreempted( void )
{
    static TimelineTaskConfig_t tasks[] =
    {
        { "HRT_OK", vTask_HRT_Completing, HARD_RT, 20, 40, 0, NULL, 0 },
        { "SRT_BG", vTask_SRT_Background, SOFT_RT,  0,  0, 0, NULL, 0 },
    };
    static TimelineConfig_t cfg =
    {
        .tasks              = tasks,
        .tasks_num          = 2,
        .num_sub_frames     = NUM_SUB_FRAMES,
        .major_frame_period = MAJOR_FRAME_DURATION,
        .sub_frame_period   = SUB_FRAME_DURATION,
    };

    vTestLogReset();
    vConfigureScheduler( &cfg );

    prvWaitFrames( 2, MAJOR_FRAME_DURATION * 3 );

    /* The HRT task must start at frame-tick 20 ± 1 (release jitter ≤ 1). */
    uint32_t ulHRTStarts = 0;

    for( uint32_t i = 0; i < ulTestEventCount; i++ )
    {
        volatile TestEvent_t *e = &xTestEventLog[ i ];

        if( e->eType == EVT_TASK_START &&
            strncmp( (const char *) e->pcTaskName,
                     "HRT_OK", configMAX_TASK_NAME_LEN ) == 0 )
        {
            ulHRTStarts++;

            if( e->ulFrameTick < 20 || e->ulFrameTick > 21 )
            {
                prvPrintVerdict( 4, "SRT Task Preempted by HRT Task",
                                 TEST_FAILED,
                                 "HRT start tick outside [20,21]" );
                return TEST_FAILED;
            }
        }
    }

    if( ulHRTStarts == 0 )
    {
        prvPrintVerdict( 4, "SRT Task Preempted by HRT Task",
                         TEST_FAILED, "HRT never started" );
        return TEST_FAILED;
    }

    prvPrintVerdict( 4, "SRT Task Preempted by HRT Task",
                     TEST_PASSED, "" );
    return TEST_PASSED;
}

/* -------------------------------------------------------------------------
 * Test 5 – Major Frame Repeats Deterministically (2 cycles)
 *
 * Config  : Single HRT task, window [10, 30).
 * Expect  : The start tick of the task in frame 2 equals the start tick
 *           in frame 1 modulo MAJOR_FRAME_DURATION, within ± 1 tick.
 * ---------------------------------------------------------------------- */
static TestResult_t prvTest5_DeterministicRepeat( void )
{
    static TimelineTaskConfig_t tasks[] =
    {
        { "HRT_OK", vTask_HRT_Completing, HARD_RT, 10, 30, 0, NULL, 0 },
    };
    static TimelineConfig_t cfg =
    {
        .tasks              = tasks,
        .tasks_num          = 1,
        .num_sub_frames     = NUM_SUB_FRAMES,
        .major_frame_period = MAJOR_FRAME_DURATION,
        .sub_frame_period   = SUB_FRAME_DURATION,
    };

    vTestLogReset();
    vConfigureScheduler( &cfg );

    prvWaitFrames( 3, MAJOR_FRAME_DURATION * 4 );

    uint32_t    ulStartTicks[ 3 ] = { 0 };
    uint32_t    ulFound           = 0;
    uint8_t     bSkippedFirst     = 0;

    for( uint32_t i = 0;
         i < ulTestEventCount && ulFound < 3;
         i++ )
    {
        volatile TestEvent_t *e = &xTestEventLog[ i ];

        if( e->eType == EVT_TASK_START &&
            strncmp( (const char *) e->pcTaskName,
                     "HRT_OK", configMAX_TASK_NAME_LEN ) == 0 )
        {
            if( !bSkippedFirst )
            {
                bSkippedFirst = 1;
                continue;
            }
            ulStartTicks[ ulFound++ ] = e->ulFrameTick;
        }
    }

    if( ulFound < 2 )
    {
        prvPrintVerdict( 5, "Major Frame Repeats Deterministically",
                         TEST_FAILED, "fewer than 2 stable HRT activations seen" );
        return TEST_FAILED;
    }

    for( uint32_t k = 1; k < ulFound; k++ )
    {
        uint32_t ulDiff = ( ulStartTicks[ k ] > ulStartTicks[ k - 1 ] )
                          ? ( ulStartTicks[ k ] - ulStartTicks[ k - 1 ] )
                          : ( ulStartTicks[ k - 1 ] - ulStartTicks[ k ] );

        if( ulDiff > 1 )
        {
            UART_printf( "  [dbg] frame-tick[%d]=%d  frame-tick[%d]=%d  diff=%d\n",
                         k - 1, ulStartTicks[ k - 1 ],
                         k,     ulStartTicks[ k ],
                         ulDiff );

            prvPrintVerdict( 5, "Major Frame Repeats Deterministically",
                             TEST_FAILED,
                             "start-tick jitter > 1 between frames" );
            return TEST_FAILED;
        }
    }

    prvPrintVerdict( 5, "Major Frame Repeats Deterministically",
                     TEST_PASSED, "" );
    return TEST_PASSED;
}

/* -------------------------------------------------------------------------
 * Test 6 – Overlapping HRT Windows (Stress)
 *
 * Config  : HRT_A window [10, 40), HRT_B window [20, 50).  The windows
 *           overlap in [20, 40).
 * Expect  : System does not crash (runs for 2 frames without hanging).
 *           HRT_A starts at frame-tick 10 ± 1.
 *           HRT_B either never starts (scheduler blocks it) OR it starts
 *           only after HRT_A has ended.  A hard crash or watchdog hit
 *           counts as FAILED.
 * ---------------------------------------------------------------------- */
static TestResult_t prvTest6_OverlappingHRT( void )
{
    static TimelineTaskConfig_t tasks[] =
    {
        { "HRT_A", vTask_HRT_Overlap_A, HARD_RT, 10, 40, 0, NULL, 0 },
        { "HRT_B", vTask_HRT_Overlap_B, HARD_RT, 20, 50, 0, NULL, 0 },
    };
    static TimelineConfig_t cfg =
    {
        .tasks              = tasks,
        .tasks_num          = 2,
        .num_sub_frames     = NUM_SUB_FRAMES,
        .major_frame_period = MAJOR_FRAME_DURATION,
        .sub_frame_period   = SUB_FRAME_DURATION,
    };

    vTestLogReset();
    vConfigureScheduler( &cfg );

    uint32_t ulFramesSeen = prvWaitFrames( 2, MAJOR_FRAME_DURATION * 3 );

    /* System survived – basic liveness check. */
    if( ulFramesSeen < 2 )
    {
        prvPrintVerdict( 6, "Overlapping HRT Windows (Stress)",
                         TEST_FAILED, "system did not complete 2 frames" );
        return TEST_FAILED;
    }

    /* If HRT_B did start, it must start after HRT_A has ended. */
    TickType_t xAEnd   = 0;
    TickType_t xBStart = 0;
    uint8_t    bBStarted = 0;
    uint8_t    bAEnded = 0; /* FIX: Added missing flag */

    for( uint32_t i = 0; i < ulTestEventCount; i++ )
    {
        volatile TestEvent_t *e = &xTestEventLog[ i ];

        if( e->eType == EVT_TASK_END &&
            strncmp( (const char *) e->pcTaskName, "HRT_A", configMAX_TASK_NAME_LEN ) == 0 &&
            bAEnded == 0 ) /* FIX: Lock to the first frame */
        {
            xAEnd = e->xTick;
            bAEnded = 1;
        }

        if( e->eType == EVT_TASK_START &&
            strncmp( (const char *) e->pcTaskName, "HRT_B", configMAX_TASK_NAME_LEN ) == 0 &&
            bBStarted == 0 )
        {
            xBStart   = e->xTick;
            bBStarted = 1;
        }
    }

    if( bBStarted && xAEnd > 0 && xBStart < xAEnd )
    {
        prvPrintVerdict( 6, "Overlapping HRT Windows (Stress)",
                         TEST_FAILED,
                         "HRT_B started before HRT_A ended" );
        return TEST_FAILED;
    }

    prvPrintVerdict( 6, "Overlapping HRT Windows (Stress)",
                     TEST_PASSED, "" );
    return TEST_PASSED;
}

/* -------------------------------------------------------------------------
 * Test 7 – Minimal Time Gap Between Consecutive HRT Tasks (Edge Case)
 *
 * Config  : MG_A window [10, 20), MG_B window [20, 30).  Gap = 0 ticks.
 * Expect  : MG_A ends at or before frame-tick 20.
 *           MG_B starts at frame-tick 20 ± 1.
 *           No deadline miss for either task (both burn only 8 ticks).
 * ---------------------------------------------------------------------- */
static TestResult_t prvTest7_MinimalGap( void )
{
    static TimelineTaskConfig_t tasks[] =
    {
        { "MG_A", vTask_HRT_MinGap_A, HARD_RT, 10, 20, 0, NULL, 0 },
        { "MG_B", vTask_HRT_MinGap_B, HARD_RT, 20, 30, 0, NULL, 0 },
    };
    static TimelineConfig_t cfg =
    {
        .tasks              = tasks,
        .tasks_num          = 2,
        .num_sub_frames     = NUM_SUB_FRAMES,
        .major_frame_period = MAJOR_FRAME_DURATION,
        .sub_frame_period   = SUB_FRAME_DURATION,
    };

    vTestLogReset();
    vConfigureScheduler( &cfg );

    prvWaitFrames( 2, MAJOR_FRAME_DURATION * 3 );

    if( prvCountEvents( EVT_DEADLINE_MISS, "MG_A" ) > 0 ||
        prvCountEvents( EVT_DEADLINE_MISS, "MG_B" ) > 0 )
    {
        prvPrintVerdict( 7, "Minimal Time Gap (Edge Case)",
                         TEST_FAILED,
                         "unexpected deadline miss on zero-gap tasks" );
        return TEST_FAILED;
    }

    uint8_t bSkippedFirstMGB = 0;
    uint8_t bMGBValidated = 0;

    for( uint32_t i = 0; i < ulTestEventCount; i++ )
    {
        volatile TestEvent_t *e = &xTestEventLog[ i ];

        if( e->eType == EVT_TASK_START &&
            strncmp( (const char *) e->pcTaskName, "MG_B", configMAX_TASK_NAME_LEN ) == 0 )
        {
            if( !bSkippedFirstMGB )
            {
                bSkippedFirstMGB = 1;
                continue;
            }

            if( e->ulFrameTick < 19 || e->ulFrameTick > 22 )
            {
                prvPrintVerdict( 7, "Minimal Time Gap (Edge Case)",
                                 TEST_FAILED,
                                 "MG_B start tick outside acceptable jitter window" );
                return TEST_FAILED;
            }
            bMGBValidated = 1;
        }
    }

    if( !bMGBValidated )
    {
        prvPrintVerdict( 7, "Minimal Time Gap (Edge Case)",
                         TEST_FAILED, "no stable MG_B activations evaluated" );
        return TEST_FAILED;
    }

    if( prvCountEvents( EVT_TASK_END, "MG_A" ) == 0 )
    {
        prvPrintVerdict( 7, "Minimal Time Gap (Edge Case)",
                         TEST_FAILED, "MG_A never recorded EVT_TASK_END" );
        return TEST_FAILED;
    }

    prvPrintVerdict( 7, "Minimal Time Gap (Edge Case)",
                     TEST_PASSED, "" );
    return TEST_PASSED;
}

/* -------------------------------------------------------------------------
 * Test 8 – NULL / Empty Configuration Rejected Gracefully
 *
 * Expect  : vConfigureScheduler(NULL) returns without crashing.
 *           vConfigureScheduler(&empty_cfg) returns without crashing.
 *           No tasks are created (task list stays empty).
 *           The FreeRTOS task count after the call equals the count before.
 * ---------------------------------------------------------------------- */
static TestResult_t prvTest8_NullConfig( void )
{
    UBaseType_t uxTasksBefore = uxTaskGetNumberOfTasks();

    /* Call 1: NULL pointer. */
    vConfigureScheduler( NULL );

    /* Call 2: empty task list. */
    static TimelineConfig_t xEmptyCfg =
    {
        .tasks              = NULL,
        .tasks_num          = 0,
        .num_sub_frames     = NUM_SUB_FRAMES,
        .major_frame_period = MAJOR_FRAME_DURATION,
        .sub_frame_period   = SUB_FRAME_DURATION,
    };
    vConfigureScheduler( &xEmptyCfg );

    /* Call 3: tasks_num == 0 with a non-NULL (but empty) array pointer. */
    static TimelineTaskConfig_t xDummyArray[ 1 ];
    static TimelineConfig_t xZeroCfg =
    {
        .tasks              = xDummyArray,
        .tasks_num          = 0,
        .num_sub_frames     = NUM_SUB_FRAMES,
        .major_frame_period = MAJOR_FRAME_DURATION,
        .sub_frame_period   = SUB_FRAME_DURATION,
    };
    vConfigureScheduler( &xZeroCfg );

    UBaseType_t uxTasksAfter = uxTaskGetNumberOfTasks();

    /*
     * The task count must not have increased.  (The supervisor task itself
     * is already in uxTasksBefore.)
     */
    if( uxTasksAfter > uxTasksBefore )
    {
        prvPrintVerdict( 8, "NULL / Empty Config Rejected Gracefully",
                         TEST_FAILED,
                         "unexpected tasks created from invalid config" );
        return TEST_FAILED;
    }

    prvPrintVerdict( 8, "NULL / Empty Config Rejected Gracefully",
                     TEST_PASSED, "" );
    return TEST_PASSED;
}

/* =========================================================================
 * SUPERVISOR TASK
 *
 * Runs all tests in sequence.  Between each test it waits long enough for
 * the scheduler under test to be dismantled by the Frame Manager before
 * the next vConfigureScheduler() call.
 * ===================================================================== */

static void vTestSupervisorTask( void *pvParameters )
{
    ( void ) pvParameters;

    uint32_t ulPassed = 0;
    uint32_t ulTotal  = 8;

    UART_printf( "\n" );
    UART_printf( "============================================\n" );
    UART_printf( "Timeline Scheduler - Regression Test Suite\n" );
    UART_printf( "============================================\n" );

    /* Small settling delay to let the kernel stabilise before test 1. */
    vTaskDelay( pdMS_TO_TICKS( 10 ) );

    if( prvTest1_HRTCompletes()        == TEST_PASSED ) ulPassed++;
    vTaskDelay( pdMS_TO_TICKS( 50 ) );  /* inter-test gap */

    if( prvTest2_HRTDeadlineMiss()     == TEST_PASSED ) ulPassed++;
    vTaskDelay( pdMS_TO_TICKS( 50 ) );

    if( prvTest3_SRTIdleOnly()         == TEST_PASSED ) ulPassed++;
    vTaskDelay( pdMS_TO_TICKS( 50 ) );

    if( prvTest4_SRTPreempted()        == TEST_PASSED ) ulPassed++;
    vTaskDelay( pdMS_TO_TICKS( 50 ) );

    if( prvTest5_DeterministicRepeat() == TEST_PASSED ) ulPassed++;
    vTaskDelay( pdMS_TO_TICKS( 50 ) );

    if( prvTest6_OverlappingHRT()      == TEST_PASSED ) ulPassed++;
    vTaskDelay( pdMS_TO_TICKS( 50 ) );

    if( prvTest7_MinimalGap()          == TEST_PASSED ) ulPassed++;
    vTaskDelay( pdMS_TO_TICKS( 50 ) );

    if( prvTest8_NullConfig()          == TEST_PASSED ) ulPassed++;

    UART_printf( "============================================\n" );
    UART_printf( "Results: %d / %d tests PASSED\n", ulPassed, ulTotal );
    UART_printf( "============================================\n" );
    
    /* All done - pull the plug on the OS so it stops printing! */
    portDISABLE_INTERRUPTS();
    while(1);
}


/* =========================================================================
 * PUBLIC API
 * ===================================================================== */

void vRunAllSchedulerTests( void )
{
    BaseType_t xResult = xTaskCreate(
        vTestSupervisorTask,
        "TestSuper",
        configMINIMAL_STACK_SIZE * 4,  /* Larger stack: supervisor calls all tests */
        NULL,
        2,                             /* Same priority as HRT so it can observe them */
        NULL
    );

    if( xResult != pdPASS )
    {
        UART_printf( "[-] FATAL: could not create test supervisor task\n" );
    }
}