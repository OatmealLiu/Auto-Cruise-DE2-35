// File: SharedMemory.c

#include <stdio.h>
#include "includes.h"
#include <string.h>

#define DEBUG 0

/* Definition of Task Stacks */
/* Stack grows from HIGH to LOW memory */
#define   TASK_STACKSIZE       2048
OS_STK    task0_stk[TASK_STACKSIZE];
OS_STK    task1_stk[TASK_STACKSIZE];

/* Definition of Task Priorities */
#define TASK0_PRIORITY      6  // highest priority
#define TASK1_PRIORITY      7

OS_EVENT *pAtomicSem0;
OS_EVENT *pAtomicSem1;

// declare the ptr to the shared memory
INT32S CommBuf;
OS_MEM *commMem;
INT32S *pShared;


/* Prints a message and sleeps for given time interval */
void task0(void* pdata)
{
    short state = 0;

    // T0: assign the initial value to the shared mem.
    while (1)
    {
        INT8U err1;
        INT8U err2;

        OSSemPend(pAtomicSem0, 0, &err1);

        if (state == 0)
        {
            (*pShared) = -1 * (*pShared) + 1;
            printf("Sending       : %d\n", (*pShared));
            err2 = OSSemPost(pAtomicSem1);
            state = 1;
        }
        else if (state == 1)
        {
            printf("Receiving     : %d\n", (*pShared));
            err2 = OSSemPost(pAtomicSem0);
            state = 0;
        }
    }
}

/* Prints a message and sleeps for given time interval */
void task1(void* pdata)
{
    // short state = 0;
    while (1)
    {
        INT8U err1;
        INT8U err2;

        OSSemPend(pAtomicSem1, 0, &err1);

        (*pShared) = (*pShared) * (-1);

        err2 = OSSemPost(pAtomicSem0);

        // if (state == 0)
        // {
        //     err2 = OSSemPost(pAtomicSem1);
        //     state = 1;
        // }
        // else if (state == 1)
        // {
        //     err2 = OSSemPost(pAtomicSem0);
        //     state = 0;
        // }
    }
}

int main(void)
{
    printf("Lab 2 - Shared Memory Communication\n");
    pAtomicSem0 = OSSemCreate(1);
    pAtomicSem1 = OSSemCreate(0);

    // Let's allocate the memory as shared memory from MicroC/OS-II flavor
    INT8U errCreate;
    INT8U errGet;

    commMem = OSMemCreate(&CommBuf, 1, 1 * sizeof(INT32S), &errCreate);

    pShared = OSMemGet(commMem, &errGet);

    (*pShared) = 0;

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

    // free the memory after use
    //INT8U *pMsg;
    //OSMemPut(pMem, pMsg);

    return 0;
}
