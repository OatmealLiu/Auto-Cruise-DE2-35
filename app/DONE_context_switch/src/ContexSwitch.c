// File: ContextSwitch.c

#include <stdio.h>
#include "system.h"
#include "includes.h"
#include <string.h>
#include "altera_avalon_performance_counter.h"

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

int pre_iteration_counter = 0;
int iteration_counter = 0;
long long int cs_overhead_ticks = 0;
double cs_avg_ticks = 0;

/* Prints a message and sleeps for given time interval */
void task0(void* pdata)
{
    short state = 0;
    while (1)
    {
        INT8U err1;
        INT8U err2;

        OSSemPend(pAtomicSem0, 0, &err1);
        if (state == 1)
        {
            // int dummy = 0;
            // while (dummy < 10000 * iteration_counter)
            // {
            //     dummy = dummy + 1;
            // }

            PERF_END(PERFORMANCE_COUNTER_BASE, 1);

            pre_iteration_counter = iteration_counter;
            // Filter the outlier out
            if (cs_avg_ticks != 0)
            {
                // printf("----------> perf_get_section_time () = %d\n", (int)perf_get_section_time((void*)PERFORMANCE_COUNTER_BASE, 1));
                int raw_cs_section_ticks = (int)perf_get_section_time((void*)PERFORMANCE_COUNTER_BASE, 1);

                if ((raw_cs_section_ticks <= 1.4 * cs_avg_ticks) && (raw_cs_section_ticks >= 0.6 * cs_avg_ticks))
                {
                    cs_overhead_ticks = cs_overhead_ticks + raw_cs_section_ticks;
                    iteration_counter = iteration_counter + 1;
                }
            }
            else
            {
                // printf("----------> perf_get_section_time () = %d\n", (int)perf_get_section_time((void*)PERFORMANCE_COUNTER_BASE, 1));
                cs_overhead_ticks = cs_overhead_ticks + \
                            (int)perf_get_section_time((void*)PERFORMANCE_COUNTER_BASE, 1);
                iteration_counter = iteration_counter + 1;
            }

            if (((iteration_counter % 10) == 0) && (pre_iteration_counter != iteration_counter))
            {
                cs_avg_ticks = (double) cs_overhead_ticks / (double)iteration_counter;
                printf("====================================================================\n");
                printf("Current AVG = %lf\n", cs_avg_ticks);
                printf("Performance Counter Overhead in ticks: %d\n", (int) cs_overhead_ticks);
                printf("Performance Counter Frequency : %d\n", (int) alt_get_cpu_freq());
                printf("Timer overhead in ms:    %f\n", 1000.0 * (float)cs_overhead_ticks/(float)alt_get_cpu_freq());
                printf("====================================================================\n");
            }
            PERF_STOP_MEASURING(PERFORMANCE_COUNTER_BASE);
        }

        // print current state
        printf("Task 0 - State %d\n", state);
        // 0 - 1 - 0 - 0 - 1 - 0
        if (state == 0)
        {
            state = 1;
            PERF_RESET(PERFORMANCE_COUNTER_BASE);
            PERF_START_MEASURING(PERFORMANCE_COUNTER_BASE);
            PERF_BEGIN(PERFORMANCE_COUNTER_BASE, 1);
            err2 = OSSemPost(pAtomicSem1);
        }
        else if (state == 1)
        {
            state = 0;
            err2 = OSSemPost(pAtomicSem0);
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
        if (state == 0)
        {
            // int dummy = 0;
            // while (dummy < 10000 * iteration_counter)
            // {
            //     dummy = dummy + 1;
            // }
            PERF_END(PERFORMANCE_COUNTER_BASE, 1);
            pre_iteration_counter = iteration_counter;

            // Filter the outlier out
            if (cs_avg_ticks != 0)
            {
                // printf("----------> perf_get_section_time () = %d\n", (int)perf_get_section_time((void*)PERFORMANCE_COUNTER_BASE, 1));
                int raw_cs_section_ticks = (int)perf_get_section_time((void*)PERFORMANCE_COUNTER_BASE, 1);

                if ((raw_cs_section_ticks <= 1.4 * cs_avg_ticks) && (raw_cs_section_ticks >= 0.6 * cs_avg_ticks))
                {
                    cs_overhead_ticks = cs_overhead_ticks + raw_cs_section_ticks;
                    iteration_counter = iteration_counter + 1;
                }
            }
            else
            {
                // printf("----------> perf_get_section_time () = %d\n", (int)perf_get_section_time((void*)PERFORMANCE_COUNTER_BASE, 1));
                cs_overhead_ticks = cs_overhead_ticks + \
                            (int)perf_get_section_time((void*)PERFORMANCE_COUNTER_BASE, 1);
                iteration_counter = iteration_counter + 1;
            }

            if (((iteration_counter % 10) == 0) && (pre_iteration_counter != iteration_counter))
            {
                cs_avg_ticks = (double) cs_overhead_ticks / (double)iteration_counter;
                printf("====================================================================\n");
                printf("Current AVG = %lf\n", cs_avg_ticks);
                printf("Performance Counter Overhead in ticks: %d\n", (int) cs_overhead_ticks);
                printf("Performance Counter Frequency : %d\n", (int) alt_get_cpu_freq());
                printf("Timer overhead in ms:    %f\n", 1000.0 * (float)cs_overhead_ticks/(float)alt_get_cpu_freq());
                printf("====================================================================\n");
            }
            PERF_STOP_MEASURING(PERFORMANCE_COUNTER_BASE);
        }

        printf("Task 1 - State %d\n", state);

        if (state == 0)
        {
            state = 1;
            err2 = OSSemPost(pAtomicSem1);
        }
        else if (state == 1)
        {
            state = 0;
            PERF_RESET(PERFORMANCE_COUNTER_BASE);
            PERF_START_MEASURING(PERFORMANCE_COUNTER_BASE);
            PERF_BEGIN(PERFORMANCE_COUNTER_BASE, 1);
            err2 = OSSemPost(pAtomicSem0);
        }
    }
}


/* The main function creates two task and starts multi-tasking */
int main(void)
{
    // Reset the counters before every run
    // PERF_RESET(PERFORMANCE_COUNTER_BASE);
    //
    // PERF_START_MEASURING(PERFORMANCE_COUNTER_BASE);
    //

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

    // PERF_STOP_MEASURING(PERFORMANCE_COUNTER_BASE);
    return 0;
}
