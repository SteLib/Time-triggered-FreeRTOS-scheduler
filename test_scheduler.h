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
 * @file  test_scheduler.h
 * @brief Public API for the timeline-scheduler regression test suite.
 *
 * Usage
 * -----
 * Call vRunAllSchedulerTests() from main() BEFORE vTaskStartScheduler().
 * It creates one supervisor task that sequences every test, then the
 * scheduler runs normally.
 *
 *   int main(void) {
 *       UART_init();
 *       vRunAllSchedulerTests();   // <-- registers tests
 *       vTaskStartScheduler();
 *   }
 *
 * Each test configures its own TimelineConfig_t, calls vConfigureScheduler(),
 * runs for a fixed number of major frames, collects observations through the
 * test_probe hooks, and prints a PASSED / FAILED verdict.
 */

#ifndef TEST_SCHEDULER_H
#define TEST_SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"
#include "scheduler.h"

/* -------------------------------------------------------------------------
 * Test result type
 * ---------------------------------------------------------------------- */
typedef enum
{
    TEST_PASSED = 0,
    TEST_FAILED = 1
} TestResult_t;

/* -------------------------------------------------------------------------
 * Per-test observation record
 *
 * The test-probe callbacks (see test_probe.h) fill one of these for every
 * task event that occurs during a test run.  The verifier function for each
 * test walks the log and checks invariants.
 * ---------------------------------------------------------------------- */
#define TEST_MAX_EVENTS     ( 128 )

typedef enum
{
    EVT_TASK_START      = 0,  /* Task became running.                   */
    EVT_TASK_END        = 1,  /* Task completed voluntarily.            */
    EVT_DEADLINE_MISS   = 2,  /* Task was forcibly terminated.          */
    EVT_FRAME_RESET     = 3,  /* Major-frame boundary reached.          */
    EVT_SRT_PREEMPTED   = 4,  /* SRT task was preempted by an HRT task. */
} EventType_t;

typedef struct
{
    EventType_t eType;
    char        pcTaskName[ configMAX_TASK_NAME_LEN ];
    TickType_t  xTick;        /* xTaskGetTickCount() at the moment.     */
    uint32_t    ulFrameTick;  /* Tick offset inside the current frame.  */
} TestEvent_t;

/* Shared log written by probes, read by verifiers. */
extern volatile TestEvent_t xTestEventLog[ TEST_MAX_EVENTS ];
extern volatile uint32_t    ulTestEventCount;

/* Reset the log between tests. */
void vTestLogReset( void );

/* Append one event (ISR-safe via taskENTER/EXIT_CRITICAL). */
void vTestLogEvent( EventType_t eType,
                    const char *pcName,
                    TickType_t  xTick,
                    uint32_t    ulFrameTick );

void vTestLogEventFromISR( EventType_t eType, const char *pcName, TickType_t xTick, uint32_t ulFrameTick );

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */

/**
 * vRunAllSchedulerTests()
 *
 * Creates the test supervisor task and returns immediately.
 * All output goes to UART in the format:
 *
 *   ============================================
 *   Timeline Scheduler – Regression Test Suite
 *   ============================================
 *   Test 1 – HRT Task Completes Within Deadline : PASSED
 *   Test 2 – HRT Task Deadline Miss / Kill       : PASSED
 *   ...
 *   ============================================
 *   Results: 7 / 8 tests PASSED
 *   ============================================
 */
void vRunAllSchedulerTests( void );

#endif /* TEST_SCHEDULER_H */