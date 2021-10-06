// File: TwoTasksImproved.c

#include <stdio.h>
#include "includes.h"
#include <string.h>

#define DEBUG 1

/* Definition of Task Stacks */
/* Stack grows from HIGH to LOW memory */
#define   TASK_STACKSIZE       2048
OS_STK    task1_stk[TASK_STACKSIZE];
OS_STK    task2_stk[TASK_STACKSIZE];
OS_STK    stat_stk[TASK_STACKSIZE];

/* Definition of Task Priorities */
#define TASK1_PRIORITY      6  // highest priority
#define TASK2_PRIORITY      7
#define TASK_STAT_PRIORITY 12  // lowest priority

OS_EVENT *pSem;

// pSem = OSSemCreate(1);

void printStackSize(char* name, INT8U prio)
{
    INT8U err;
    OS_STK_DATA stk_data;

    err = OSTaskStkChk(prio, &stk_data);
    if (err == OS_NO_ERR)
    {
        if (DEBUG == 1)
        {
            printf("%s (priority %d) - Used: %d; Free: %d\n",
               name, prio, stk_data.OSUsed, stk_data.OSFree);
        }
    }
    else
    {
      if (DEBUG == 1)
      {
          printf("Stack Check Error!\n");
      }
    }
}

// pSem = OSSemCreate(1);

/* Prints a message and sleeps for given time interval */
void task1(void* pdata)
{
    while (1)
    {
        INT8U err1;
        INT8U err2;
        char text1[] = "Hello from Task1\n";
        int i;

        // *pEvent, timeout, *perr)
        // get the resource
        OSSemPend(pSem, 0, &err1);

        for (i = 0; i < strlen(text1); i++)
            putchar(text1[i]);

        // release the resource
        err2 = OSSemPost(pSem);

        // execute the idle time of task 1
        OSTimeDlyHMSM(0, 0, 0, 11); /* Context Switch to next task
                    * Task will go to the ready state
                    * after the specified delay*/
    }
}

/* Prints a message and sleeps for given time interval */
void task2(void* pdata)
{
    while (1)
    {
        INT8U err1;
        INT8U err2;
        char text1[] = "Hello from Task2\n";
        int i;

        // *pEvent, timeout, *perr)
        // get the resource
        OSSemPend(pSem, 0, &err1);

        for (i = 0; i < strlen(text1); i++)
            putchar(text1[i]);

        // release the resource
        err2 = OSSemPost(pSem);

        // execute the idle time of task 2
        OSTimeDlyHMSM(0, 0, 0, 4); /* Context Switch to next task
                    * Task will go to the ready state
                    * after the specified delay*/
    }
}


/* Printing Statistics */
void statisticTask(void* pdata)
{
    while (1)
    {
        INT8U err1;
        INT8U err2;
        // *pEvent, timeout, *perr)
        OSSemPend(pSem, 0, &err1);

        // execute the task body
        printStackSize("Task1", TASK1_PRIORITY);
        printStackSize("Task2", TASK2_PRIORITY);
        printStackSize("StatisticTask", TASK_STAT_PRIORITY);

        // release the semaphore
        err2 = OSSemPost(pSem);
    }
}

/* The main function creates two task and starts multi-tasking */
int main(void)
{

    printf("Lab 2 - Two Tasks Improved - Accessing a Shared Resource: I/O\n");
    pSem = OSSemCreate(1);

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

    OSTaskCreateExt(
        task2,                        // Pointer to task code
        NULL,                         // Pointer to argument passed to task
        &task2_stk[TASK_STACKSIZE-1], // Pointer to top of task stack
        TASK2_PRIORITY,               // Desired Task priority
        TASK2_PRIORITY,               // Task ID
        &task2_stk[0],                // Pointer to bottom of task stack
        TASK_STACKSIZE,               // Stacksize
        NULL,                         // Pointer to user supplied memory (not needed)
        OS_TASK_OPT_STK_CHK |         // Stack Checking enabled
        OS_TASK_OPT_STK_CLR           // Stack Cleared
    );

    if (DEBUG == 1)
    {
        OSTaskCreateExt(
            statisticTask,                // Pointer to task code
  	        NULL,                         // Pointer to argument passed to task
  	        &stat_stk[TASK_STACKSIZE-1],  // Pointer to top of task stack
  	        TASK_STAT_PRIORITY,           // Desired Task priority
  	        TASK_STAT_PRIORITY,           // Task ID
  	        &stat_stk[0],                 // Pointer to bottom of task stack
  	        TASK_STACKSIZE,               // Stacksize
  	        NULL,                         // Pointer to user supplied memory (not needed)
  	        OS_TASK_OPT_STK_CHK |         // Stack Checking enabled
  	        OS_TASK_OPT_STK_CLR           // Stack Cleared
  	    );
    }

  OSStart();
  return 0;
}
