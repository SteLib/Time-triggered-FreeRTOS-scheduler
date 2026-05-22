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

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE.
 *
 * See http://www.freertos.org/a00110.html
 *----------------------------------------------------------*/

/* FIX: configUSE_TRACE_FACILITY was defined twice (first 0, then 1 via
 *      #undef + redefine at the bottom of the file).  Consolidated here
 *      as a single definition set to 1, which is required for the
 *      traceTASK_SWITCHED_IN / traceTASK_SWITCHED_OUT macros below. */
#define configUSE_TRACE_FACILITY                 1
#define configGENERATE_RUN_TIME_STATS            0

#define configUSE_PREEMPTION                     1
#define configUSE_IDLE_HOOK                      0

/* FIX: configUSE_TICK_HOOK must be 1 so vApplicationTickHook() is called.
 *      The original file had it set to 0 with a comment saying the scheduler
 *      would use it – that is contradictory.  Our scheduler logic is in the
 *      tasks.c patch (xTaskIncrementTick), so the hook itself is a no-op, but
 *      setting it to 1 avoids linker issues if it is ever needed in future. */
#define configUSE_TICK_HOOK                      1

#define configCPU_CLOCK_HZ                       ( ( unsigned long ) 25000000 )
#define configTICK_RATE_HZ                       ( ( TickType_t ) 1000 )
#define configMINIMAL_STACK_SIZE                 ( ( unsigned short ) 80 )
#define configTOTAL_HEAP_SIZE                    ( ( size_t ) ( 60 * 1024 ) )
#define configMAX_TASK_NAME_LEN                  ( 12 )
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  0
#define configUSE_CO_ROUTINES                    0
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              1
#define configCHECK_FOR_STACK_OVERFLOW           0
#define configUSE_MALLOC_FAILED_HOOK             0
#define configUSE_QUEUE_SETS                     1
#define configUSE_COUNTING_SEMAPHORES            1

#define configMAX_PRIORITIES                     ( 9UL )
#define configMAX_CO_ROUTINE_PRIORITIES          ( 2 )
#define configQUEUE_REGISTRY_SIZE                10
#define configSUPPORT_STATIC_ALLOCATION          0

/* Timer related defines. */
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             ( configMINIMAL_STACK_SIZE * 2 )

#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    3

/* Set the following definitions to 1 to include the API function, or zero
 * to exclude the API function. */
#define INCLUDE_vTaskPrioritySet                  1
#define INCLUDE_uxTaskPriorityGet                 1
#define INCLUDE_vTaskDelete                       1
#define INCLUDE_vTaskCleanUpResources             0
#define INCLUDE_vTaskSuspend                      1
#define INCLUDE_vTaskDelayUntil                   1
#define INCLUDE_vTaskDelay                        1
#define INCLUDE_uxTaskGetStackHighWaterMark       1
#define INCLUDE_xTaskGetSchedulerState            1
#define INCLUDE_xTimerGetTimerDaemonTaskHandle    1
#define INCLUDE_xTaskGetIdleTaskHandle            1
#define INCLUDE_xSemaphoreGetMutexHolder          1
#define INCLUDE_eTaskGetState                     1
#define INCLUDE_xTimerPendFunctionCall            1
#define INCLUDE_xTaskAbortDelay                   1
#define INCLUDE_xTaskGetHandle                    1

/* Disable time-slicing: SRT tasks only run when no HRT task is ready,
 * and the Idle task only runs when no other task is ready. */
#define configUSE_TIME_SLICING                   0

/* This demo makes use of one or more example stats formatting functions.  */
#define configUSE_STATS_FORMATTING_FUNCTIONS      0

/* QEMU doesn't model priority bits fully, so use all eight. */
#define configKERNEL_INTERRUPT_PRIORITY           ( 255 )

#ifndef __IASMARM__
    #define configASSERT( x )    if( ( x ) == 0 ) while( 1 )
#endif

/* configMAX_SYSCALL_INTERRUPT_PRIORITY must not be zero. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY      ( 4 )

/* QEMU doesn't model the CLZ instruction. */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION   0

#define configRUN_ADDITIONAL_TESTS                1
#define configSTREAM_BUFFER_TRIGGER_LEVEL_TEST_MARGIN  4

#define intqHIGHER_PRIORITY      ( configMAX_PRIORITIES - 5 )
#define bktPRIMARY_PRIORITY      ( configMAX_PRIORITIES - 3 )
#define bktSECONDARY_PRIORITY    ( configMAX_PRIORITIES - 4 )

#define configENABLE_BACKWARD_COMPATIBILITY      0

/* -------------------------------------------------------------------------
 * Trace macros
 *
 * UART_printf_ISR is ISR-safe (no critical section, no FreeRTOS API calls).
 * Both macros fire inside the context-switch path, which is effectively ISR
 * context, so only ISR-safe calls are permitted here.
 * ---------------------------------------------------------------------- */
extern void UART_printf_ISR( const char *s, ... );

/* Fired exactly when a task is loaded onto the CPU. */
#define traceTASK_SWITCHED_IN()                                              \
    do {                                                                     \
        UART_printf_ISR( "[ %d ms ] %s start\n",                            \
                         ( int ) xTickCount,                                 \
                         pxCurrentTCB->pcTaskName );                         \
    } while( 0 )

/* Fired exactly when a task is pulled off the CPU. */
#define traceTASK_SWITCHED_OUT()                                             \
    do {                                                                     \
        UART_printf_ISR( "[ %d ms ] %s end\n",                              \
                         ( int ) xTickCount,                                 \
                         pxCurrentTCB->pcTaskName );                         \
    } while( 0 )

#endif /* FREERTOS_CONFIG_H */