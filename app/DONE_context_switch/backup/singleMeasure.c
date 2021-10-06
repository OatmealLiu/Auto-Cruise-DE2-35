// File: ContextSwitch.c

#include <stdio.h>
#include "includes.h"
#include <string.h>
#include "altera_avalon_performance_counter.h"
#include "system.h"

#define DEBUG 0

/* Definition of Task Stacks */
/* Stack grows from HIGH to LOW memory */
#define   TASK_STACKSIZE       2048
OS_STK    task0_stk[TASK_STACKSIZE];
OS_STK    task1_stk[TASK_STACKSIZE];
// OS_STK    stat_stk[TASK_STACKSIZE];

/* Definition of Task Priorities */
#define TASK0_PRIORITY      6  // highest priority
#define TASK1_PRIORITY      7

OS_EVENT *pAtomicSem0;
OS_EVENT *pAtomicSem1;

/* Prints a message and sleeps for given time interval */
void task0(void* pdata)
{
    short state = 0;
    while (1)
    {
        INT8U err1;
        INT8U err2;

        OSSemPend(pAtomicSem0, 0, &err1);
        // 0th start
        PERF_BEGIN(PERF_UNIT_BASE, 1);

        // print current state
        printf("Task 0 - State %d\n", state);

        if (state == 0)
        {
            state = 1;
            err2 = OSSemPost(pAtomicSem1);
            PERF_END(PERF_UNIT_BASE, 1);
            // 0th end
        }
        else if (state == 1)
        {
            state = 0;
            err2 = OSSemPost(pAtomicSem0);
            PERF_END(PERF_UNIT_BASE, 1);
            // 1th end
        }
    }
}

/* Prints a message and sleeps for given time interval */
void task1(void* pdata)
{
    short state = 0;
    while (1)
    {
        INT8U err1;
        INT8U err2;

        OSSemPend(pAtomicSem1, 0, &err1);
        // 1st begin
        PERF_BEGIN(PERF_UNIT_BASE, 2);

        printf("Task 1 - State %d\n", state);

        if (state == 0)
        {
            state = 1;
            err2 = OSSemPost(pAtomicSem1);
            // 1st end
            PERF_END(PERF_UNIT_BASE, 2);
        }
        else if (state == 1)
        {
            state = 0;
            err2 = OSSemPost(pAtomicSem0);
            // 1st end
            PERF_END(PERF_UNIT_BASE, 2);
        }
    }
}

/* The main function creates two task and starts multi-tasking */
int main(void)
{
    // Reset the counters before every run
    PERF_RESET(PERF_UNIT_BASE);
    //PERF_RESET(PERFORMANCE_COUNTER_BASE);

    PERF_START_MEASURING(PERF_UNIT_BASE);


    printf("Lab 2 - Context Switch\n");
    pAtomicSem0 = OSSemCreate(1);
    pAtomicSem1 = OSSemCreate(0);

    OSTaskCreateExt(
        task0,                        // Pointer to task code
        NULL,                         // Pointer to argument passed to task
        &task0_stk[TASK_STACKSIZE-1], // Pointer to top of task stack
        TASK0_PRIORITY,               // Desired Task priority
        TASK0_PRIORITY,               // Task ID
        &task0_stk[0],                // Pointer to bottom of task stack
        TASK_STACKSIZE,               // Stacksize
        NULL,                         // Pointer to user supplied memory (not needed)
        OS_TASK_OPT_STK_CHK |         // Stack Checking enabled
        OS_TASK_OPT_STK_CLR           // Stack Cleared
    );

    OSTaskCreateExt(
        task1,                        // Pointer to task code
        NULL,                         // Pointer to argument passed to task
        &task1_stk[TASK_STACKSIZE-1], // Pointer to top of task stack
        TASK1_PRIORITY,               // Desired Task priority
        TASK1_PRIORITY,               // Task ID
        &task1_stk[0],                // Pointer to bottom of task stack
        TASK_STACKSIZE,               // Stacksize
        NULL,                         // Pointer to user supplied memory (not needed)
        OS_TASK_OPT_STK_CHK |         // Stack Checking enabled
        OS_TASK_OPT_STK_CLR           // Stack Cleared
    );

    OSStart();

    PERF_STOP_MEASURING(PERF_UNIT_BASE);

    perf_print_formatted_report(
        PERF_UNIT_BASE,                                 // performance counter base address
        alt_get_cpu_freq(),                             // clock frequency
        2,                                              // the number of section counters to display
        "Task 0Contact Switch",                         // 1st section name to display
        "Task 1 Contact Switch"                         // 2nd section name to display
    );
    return 0;
}
