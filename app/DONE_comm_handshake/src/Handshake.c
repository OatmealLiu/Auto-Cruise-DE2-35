// File: Handshake.c

#include <stdio.h>
#include "includes.h"
#include <string.h>

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

        // print current state
        printf("Task 0 - State %d\n", state);

        if (state == 0)
        {
            err2 = OSSemPost(pAtomicSem1);
            state = 1;
        }
        else if (state == 1)
        {
            err2 = OSSemPost(pAtomicSem0);
            state = 0;
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

        printf("Task 1 - State %d\n", state);

        if (state == 0)
        {
            err2 = OSSemPost(pAtomicSem1);
            state = 1;
        }
        else if (state == 1)
        {
            err2 = OSSemPost(pAtomicSem0);
            state = 0;
        }
    }
}

/* The main function creates two task and starts multi-tasking */
int main(void)
{
    printf("Lab 2 - Communication by Handshakes\n");
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
    return 0;
}
