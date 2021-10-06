/* Cruise control skeleton for the IL 2206 embedded lab
 *
 * Maintainers:  Rodolfo Jordao (jordao@kth.se), George Ungereanu (ugeorge@kth.se)
 *
 * Description:
 *
 *   In this file you will find the "model" for the vehicle that is being simulated on top
 *   of the RTOS and also the stub for the control task that should ideally control its
 *   velocity whenever a cruise mode is activated.
 *
 *   The missing functions and implementations in this file are left as such for
 *   the students of the IL2206 course. The goal is that they get familiriazed with
 *   the real time concepts necessary for all implemented herein and also with Sw/Hw
 *   interactions that includes HAL calls and IO interactions.
 *
 *   If the prints prove themselves too heavy for the final code, they can
 *   be exchanged for alt_printf where hexadecimals are supported and also
 *   quite readable. This modification is easily motivated and accepted by the course
 *   staff.
 */
#include <stdio.h>
#include "system.h"
#include "includes.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"
#include "sys/alt_alarm.h"

#define DEBUG 1

#define HW_TIMER_PERIOD 100 /* 100ms */

/* Button Patterns */
#define GAS_PEDAL_FLAG      0x08
#define BRAKE_PEDAL_FLAG    0x04
#define CRUISE_CONTROL_FLAG 0x02

/* Switch Patterns */
#define TOP_GEAR_FLAG       0x00000002
#define ENGINE_FLAG         0x00000001

/* LED Patterns */
#define LED_RED_0 0x00000001    // Engine
#define LED_RED_1 0x00000002    // Top Gear

#define LED_GREEN_0 0x0001      // Cruise Control activated
#define LED_GREEN_2 0x0002      // Cruise Control Button
#define LED_GREEN_4 0x0010      // Brake Pedal
#define LED_GREEN_6 0x0040      // Gas Pedal

/*
 * Definition of Tasks
 */

//      |- Standard task stacksize
#define TASK_STACKSIZE 2048

// Declare stacksize for each task
//      |- Original task stacksize
OS_STK StartTask_Stack[TASK_STACKSIZE];
OS_STK ControlTask_Stack[TASK_STACKSIZE];
OS_STK VehicleTask_Stack[TASK_STACKSIZE];
//      |- Added task stacksize
OS_STK SwitchIOTask_Stack[TASK_STACKSIZE];
OS_STK KeyIOTask_Stack[TASK_STACKSIZE];
OS_STK OverloadDetectionTask_Stack[TASK_STACKSIZE];
OS_STK WatchdogTask_Stack[TASK_STACKSIZE];
OS_STK ExtraLoadTask_Stack[TASK_STACKSIZE];


//-- Task Priorities
//      := Original task priorities

// NOTE: OG config.
#define STARTTASK_PRIO                  5

#define VEHICLETASK_PRIO               10
#define CONTROLTASK_PRIO               12
//      := Added tasks priorities
#define WATCHDOGTASK_PRIO               6

#define SWITCHIOTASK_PRIO               7
#define KEYIOTASK_PRIO                  8

#define EXTRALOADTASK_PRIO             13
#define OVERLOADDETECTIONTASK_PRIO     15

// // NOTE: Test config.DEBUG
// #define STARTTASK_PRIO                  5
//
// #define VEHICLETASK_PRIO               10
// #define CONTROLTASK_PRIO               12
// //      := Added tasks priorities
// #define WATCHDOGTASK_PRIO               6
//
// #define SWITCHIOTASK_PRIO               13
// #define KEYIOTASK_PRIO                  14
//
// #define EXTRALOADTASK_PRIO             15
// #define OVERLOADDETECTIONTASK_PRIO     16

//-- Task Periods
#define CONTROL_PERIOD  300
#define VEHICLE_PERIOD  300
#define SWITCHIO_PERIOD 150 //DEBUG 150
#define KEYIO_PERIOD    150 //DEBUG 150
#define HYPER_PERIOD    300

/*
 * Definition of Kernel Objects
 */

//-- Mailboxes
OS_EVENT *Mbox_Throttle;
OS_EVENT *Mbox_Velocity;
OS_EVENT *Mbox_Engine;
OS_EVENT *Mbox_Gear;
OS_EVENT *Mbox_Cruise;
OS_EVENT *Mbox_Brake;
OS_EVENT *Mbox_Gas;

OS_EVENT *Mbox_Overload;
OS_EVENT *Mbox_ExtraLoadCreation;


//-- Periodic Tasks Scheduling Tools
//      |- Semaphores
OS_EVENT *Sem_Vehicle;
OS_EVENT *Sem_Control;
OS_EVENT *Sem_SwitchIO;
OS_EVENT *Sem_KeyIO;
OS_EVENT *Sem_OverloadDetection;
OS_EVENT *Sem_ExtraLoad;

//      |- SW-Timers
OS_TMR *Timer_Vehicle;
OS_TMR *Timer_Control;
OS_TMR *Timer_SwitchIO;
OS_TMR *Timer_KeyIO;
OS_TMR *Timer_OverloadDetection;
OS_TMR *Timer_ExtraLoad;

/*
 * Types
 */
enum active {on = 2, off = 1};


/*
 * Global variables
 */
int delay; // Delay of HW-timer
INT16U led_green = 0; // Green LEDs
INT32U led_red = 0;   // Red LEDs


/*
 * Helper functions
 */

int buttons_pressed(void)
{
    return ~IORD_ALTERA_AVALON_PIO_DATA(D2_PIO_KEYS4_BASE);
}

int switches_pressed(void)
{
    return IORD_ALTERA_AVALON_PIO_DATA(DE2_PIO_TOGGLES18_BASE);
}

/*
 * ISR for HW Timer
 */
alt_u32 alarm_handler(void* context)
{
    OSTmrSignal(); /* Signals a 'tick' to the SW timers */
    return delay;
}

static int b2sLUT[] = {
    0x40, //0
    0x79, //1
    0x24, //2
    0x30, //3
    0x19, //4
    0x12, //5
    0x02, //6
    0x78, //7
    0x00, //8
    0x18, //9
    0x3F, //-
};

/*
 * convert int to seven segment display format
 */
int int2seven(int inval)
{
    return b2sLUT[inval];
}

/*
 * output current velocity on the seven segement display
 */
void show_velocity_on_sevenseg(INT8S velocity)
{
    int tmp = velocity;
    int out;
    INT8U out_high = 0;
    INT8U out_low = 0;
    INT8U out_sign = 0;

    if(velocity < 0)
    {
        out_sign = int2seven(10);
        tmp *= -1;
    }
    else
    {
        out_sign = int2seven(0);
    }

    out_high = int2seven(tmp / 10);
    out_low = int2seven(tmp - (tmp / 10) * 10);

    out = int2seven(0) << 21 |
              out_sign << 14 |
              out_high << 7  |
              out_low;
    // HEX 0, 1, 2, 3
    IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_HEX_LOW28_BASE, out);
}

/*
 * shows the target velocity on the seven segment display (HEX5, HEX4)
 * when the cruise control is activated (0 otherwise)
 */
void show_target_velocity(INT8U target_vel)
{
    int tmp = target_vel;
    int out;
    INT8U out_high = 0;
    INT8U out_low = 0;
    INT8U out_sign = 0;

    if (target_vel < 0)
    {
        out_sign = int2seven(10);
        tmp *= -1;
    }
    else
    {
        out_sign = int2seven(0);
    }

    out_high = int2seven(tmp / 10);
    out_low = int2seven(tmp - (tmp / 10) * 10);

    out = int2seven(0) << 21 |
              out_sign << 14 |
              out_high << 7  |
              out_low;

    // HEX5, HEX4 represented by HEX_HIGH28
	IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_HEX_HIGH28_BASE, out);
}

/*
 * indicates the position of the vehicle on the track with the four leftmost red LEDs
 * LEDR17: [0m, 400m)
 * LEDR16: [400m, 800m)
 * LEDR15: [800m, 1200m)
 * LEDR14: [1200m, 1600m)
 * LEDR13: [1600m, 2000m)
 * LEDR12: [2000m, 2400m]
 * Total: 6 x LEDRs
 */
 /*
 * 17     16      15      14      13      12      11      10      9      8      7      6      5      4      3      2      1      0   *
 * 2^17   2^16    2^15    2^14    2^13    2^12    2^11    2^10    2^9    2^8    2^7    2^6    2^5    2^4    2^3    2^2    2^1    2^0 *
 *
*/
void show_position(INT16U position)
{
    // Config. Info
    // Starting point  := LEDR12 = 2^12 = 000001000000000000 = 0x1000
    // Ending point    := LEDR17 = 2^17 = 100000000000000000 = 0x20000
    // Distance / LEDR := 400 m
    int led_red_pos_config = 0x1000;
    const int shift_threshold = 400;

    // Take advantage of the rule of integer division in C
    // Left shift 1 bit of LedR_ID if and only if position is increased by 400
    // which means that, the i-th bit of the i-th LEDR will be switched to 1
    // e.g.:
    //      led_red_pos_config = 0x1000 = 2^12 = 000001000000000000
    //      led_red_pos_config = LedR_ID << 1
    //      now, led_red_pos_config = 0x2000   = 000010000000000000
    //      So, 13-th LEDR is turned on
    led_red_pos_config = led_red_pos_config << (position / shift_threshold);

    // Update global led_red config
    // by doing OR opt. btw. current global config led_red and new led_red_pos_config
    led_red = led_red | led_red_pos_config;
}

//-- Periodic Tasks Scheduling Tools
//      |- Timer Callback functions
//          := Use OSSemPost() to release the Semaphore through callback funcs
//          := when the periodic timer expires
//          := which means that, we release the scheduled job-to-be-done

void Timer_Vehicle_CallbackSemPost()
{
    INT8U err;
    err = OSSemPost(Sem_Vehicle);
}

void Timer_Control_CallbackSemPost()
{
    INT8U err;
    err = OSSemPost(Sem_Control);
}

void Timer_SwitchIO_CallbackSemPost()
{
    INT8U err;
    err = OSSemPost(Sem_SwitchIO);
}

void Timer_KeyIO_CallbackSemPost()
{
    INT8U err;
    err = OSSemPost(Sem_KeyIO);
}

void Timer_OverloadDetection_CallbackSemPost()
{
    INT8U err;
    err = OSSemPost(Sem_OverloadDetection);
}

void Timer_ExtraLoad_CallbackSemPost()
{
    INT8U err;
    err = OSSemPost(Sem_ExtraLoad);
}


//      |- Task Body Definition
/*
 * The task 'VehicleTask' is the model of the vehicle being simulated. It updates variables like
 * acceleration and velocity based on the input given to the model.
 *
 * The car model is equivalent to moving mass with linear resistances acting upon it.
 * Therefore, if left one, it will stably stop as the velocity converges to zero on a flat surface.
 * You can prove that easily via basic LTI systems methods.
 */

 //         |- VehicleTask() Body
void VehicleTask(void* pdata)
{
    // constants that should not be modified
    const unsigned int wind_factor = 1;
    const unsigned int brake_factor = 4;
    const unsigned int gravity_factor = 2;

    // variables relevant to the model and its simulation on top of the RTOS
    INT8U err;
    void* msg;
    INT8U* throttle;
    INT16S acceleration;
    INT16U position = 0;
    INT16S velocity = 0;
    enum active brake_pedal = off;
    enum active engine = off;
    INT16U local_led_green = 0; // Green LEDs
    INT32U local_led_red = 0;   // Red LEDs

    printf("Vehicle task created!\n");

    while(1)
    {
        // Send velocity msg to Mbox
        err = OSMboxPost(Mbox_Velocity, (void *) &velocity);

        // OSTimeDlyHMSM(0,0,0,VEHICLE_PERIOD);
        // Wait for the timer releasing the semaphore
        OSSemPend(Sem_Vehicle, 0, err);


        // Get vehicle components status
        //      |- throttle     := opening
        //      |- brake pedal  := ON / OFF
        //      |- engine       := ON / OFF
        /* Non-blocking read of mailbox:
           - message in mailbox: update throttle
           - no message:         use old throttle
         */
        msg = OSMboxPend(Mbox_Throttle, 1, &err);
        if (err == OS_NO_ERR)
        {
            throttle = (INT8U*) msg;
        }

        /* Same for the brake signal that bypass the control law */
        msg = OSMboxPend(Mbox_Brake, 1, &err);
        if (err == OS_NO_ERR)
        {
            //brake_pedal = (enum active) msg;    // OG Bug ???
            brake_pedal = *((enum active*) msg);
        }

        /* Same for the engine signal that bypass the control law */
        msg = OSMboxPend(Mbox_Engine, 1, &err);
        if (err == OS_NO_ERR)
        {
            //engine = (enum active) msg;       /// OG Bug ???
            engine = *((enum active*) msg);
        }


        // vehichle cannot effort more than 80 units of throttle
        if (*throttle > 80)
        {
            *throttle = 80;
        }

        // brakes + wind
        if (brake_pedal == off)
        {
            // wind resistance
            acceleration = - wind_factor * velocity;
            // actuate with engines
            if (engine == on)
            {
                acceleration += (*throttle);
            }

            // gravity effects
            if (400 <= position && position < 800)
            {
                acceleration -= gravity_factor;                 // traveling uphill
            }
            else if (800 <= position && position < 1200)
            {
                acceleration -= 2 * gravity_factor;             // traveling steep uphill
            }
            else if (1600 <= position && position < 2000)
            {
                acceleration += 2 * gravity_factor;             //traveling downhill
            }
            else if (2000 <= position)
            {
                acceleration += gravity_factor;                 // traveling steep downhill
            }
        }
        else
        {
            // if the engine and the brakes are activated at the same time,
            // we assume that the brake dynamics dominates, so both cases fall
            // here.
            acceleration = -brake_factor * velocity;
        }

        printf("Position: %d m\n", position);
        printf("Velocity: %d m/s\n", velocity);
        printf("Accell: %d m/s2\n", acceleration);
        printf("Throttle: %d V\n", *throttle);

        position = position + velocity * VEHICLE_PERIOD / 1000;
        velocity = velocity  + acceleration * VEHICLE_PERIOD / 1000.0;

        // Close the loop of the track
        // Reset the position to the starting point
        if (position > 2400)
        {
            position = 0;
        }

        // Display current velocity on LCDs
        show_velocity_on_sevenseg((INT8S) velocity);

        // Update current position onto global Red LEDs configs.
        show_position(position);

        // if ( (led_red << 6) != local_led_red )
        // {
        //     local_led_red = led_red;
        //     IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_REDLED18_BASE, led_red);
        // }
        // Display Red LEDs according to the current global led_red config.
        IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_REDLED18_BASE, led_red);

        // if ( led_green != local_led_green )
        // {
        //     local_led_green = led_green;
        //     IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_REDLED18_BASE, led_red);
        // }
        // Display Green LEDs according to the current global led_green config.
        IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_GREENLED9_BASE, led_green);

        // Reset global led_red config to 0, means all OFF
        led_red = 0x0;
        // Reset global led_green config to 0, means all OFF
        led_green = 0x0;
    }
}

/*
 * The task 'ControlTask' is the main task of the application. It reacts
 * on sensors and generates responses.
 */
 //         |- ControlTask() Body
void ControlTask(void* pdata)
{
    INT8U err;
    INT8U throttle = 40; /* Value between 0 and 80, which is interpreted as between 0.0V and 8.0V */
    void* msg;
    INT16S* current_velocity;

    INT8U target_velocity = 0;               // added



    enum active gas_pedal = off;
    enum active top_gear = off;
    enum active cruise_control = off;

    enum active engine = off;               // added
    enum active brake_pedal = off;          // added

    enum active auto_cruise = off;         // added

    INT8U Kp = 1;           // speed proportional control gain
    INT8U dt = 1;           // time tick
    INT16S delta_u = 0;     // second term of PI-Controller

    printf("Control Task created!\n");

    while(1)
    {
        // Scanning the signals from Sensors
        //      |- Current Velocity scanning
        msg = OSMboxPend(Mbox_Velocity, 0, &err);
        current_velocity = (INT16S*) msg;

        // ----- My Code Start -----
        //      |- Engine status scanning
        msg = OSMboxPend(Mbox_Engine, 0, &err);
        engine = *((enum active*) msg);

        //      |- Gear status scanning
        msg = OSMboxPend(Mbox_Gear, 0, &err);
        top_gear = *((enum active*) msg);

        //      |- Cruise Control status scanning
        msg = OSMboxPend(Mbox_Cruise, 0, &err);
        cruise_control = *((enum active*) msg);

        //      |- Brake Pedal status scanning
        msg = OSMboxPend(Mbox_Brake, 0, &err);
        brake_pedal = *((enum active*) msg);

        //      |- Gas Pedal status scanning
        msg = OSMboxPend(Mbox_Gas, 0, &err);
        gas_pedal = *((enum active*) msg);

        // throttle = 0;

        // Law - 1: Engine Control
        // The user cannot close the engine if current velocity > 0 m/s
        if (engine == off && *current_velocity > 0)
        {
            engine = on;
        }
        // Update the global led_red config for ENGINE status
        if (engine == on)
        {
            led_red = led_red | LED_RED_0;
        }

        // Law - 2: Auto-Cruise Control
        // Check if open Cruise Control
        // Auto-Cruise Pre-conditions:
        //      - cruise_control        ON
        //      - top_gear              ON
        //      - current_velocity      >= 20 m/s
        //      - brake_pedal           OFF
        //      - gas_peadl             OFF
        if (cruise_control == on && auto_cruise == off)
        {
            auto_cruise = on;
            target_velocity = (INT8U) *current_velocity;
            if (target_velocity < 29)
            {
                target_velocity = 29;
            }
        }

        if (auto_cruise == on && top_gear == on \
                                 && *current_velocity >= 20 \
                                 && brake_pedal == off \
                                 && gas_pedal == off
                                 // && cruise_control == on
                             )
        {
            // Like the vehicle cruise control system in reality
            // If the user press the Cruise Control Key,
            // they want to fix the current velocity for cruise

            // If all the pre-condition checks are passed
            // the Cruise Control function is activated

            // Starting Auto-Cruise Function
            // Set current velocity as target_velocity
            // target_velocity = (INT8U) *current_velocity;
            // if (target_velocity < 29)
            //     target_velocity = 29;

            // Turn ON LED_GREEN_2 for Auto-Cruise
            led_green = led_green | LED_GREEN_2; //CruiseControll led

            // Run the Control Law of Auto-Cruise
            // Here we use a PI-controller to control the throttle
            delta_u = Kp * (target_velocity - (INT8U) *current_velocity);
            throttle = throttle + delta_u * dt;
        }
        else
        {
            auto_cruise = off;      // DEBUG
            target_velocity = 0;
            delta_u = 0;
        }

        // Law - 3: Gas Pedal Control
        // The user can only activate the Gas Pedal when the engine is ON
        // Assumption in manual control:
        //      - Top Gear --> 60 throttle
        //      - Low Gear --> 30 throttle
        //      NOTE: we do not want to touch the maximum throttle
        if (gas_pedal == on && engine == on)
        {
            // Turn ON the Gas Pedal green LED
            led_green = led_green | LED_GREEN_6;
            if(top_gear == on)
            {
                throttle = 60;
            }
            else
            {
                throttle = 30;
            }
        }

        // Law - 4: Brake Pedal Control
        // Assumption in manual control:
        //      - Turn off the throttle if the vehicle receive Brake signal
        if (brake_pedal == on)
        {
            throttle = 0;
        }

        // ----- Control Law End -----

        // Sent the controls to their Mboxes
        err = OSMboxPost(Mbox_Throttle, (void *) &throttle);
        err = OSMboxPost(Mbox_Engine, (void *) &engine);

        // Should I return them to the MailBoxes ?
        // err = OSMboxPost(Mbox_Velocity, (void *) &current_velocity);//<---NOTE
        // err = OSMboxPost(Mbox_Gas, (void *) &gas_pedal);
        // err = OSMboxPost(Mbox_Brake, (void *) &brake_pedal);//<---NOTE
        // err = OSMboxPost(Mbox_Gear, (void *) &top_gear);//<---NOTE
        // err = OSMboxPost(Mbox_Cruise, (void *) &cruise_control);

        show_target_velocity(target_velocity);

        // OSTimeDlyHMSM(0,0,0, CONTROL_PERIOD);
        OSSemPend(Sem_Control, 0, &err);
    }
}

//         |- SwitchIOTask() Body
void SwitchIOTask(void* pdata)
{
    INT8U err;

    enum active Signal_Engine = off;
    enum active Signal_TopGear = off;

    INT16U Signal_ExtraLoadAdjustIO;
    int extraload_check_mask = 0x3F0;

    int event_SwitchIOs;

    while (1)
    {
        OSSemPend(Sem_SwitchIO, 0, &err);
        // SwitchIOs scanning begin
        // switches_pressed() return the scanned SwitchIOs config in bit-wise
        event_SwitchIOs = switches_pressed();
        Signal_ExtraLoadAdjustIO = 0;

        /* Bit Operation Description
         * Use Bit operation to check each SwitchIOs' status
         * e.g.: Engine Check
         *      ENGINE_FLAG = 0x00000001 = 2^0
         *      Use ENGINE_FLAG as a Mask
         *      Do AND opt. on current SwitchIOs config 'event_SwitchIOs'
         *      Other bits related to other IOs will be filtered out
         *      If the bit represent the ENGINE ON is 1, then signal engine on
         *      Otherwise, signal engine OFF
         */
        //      |- ENGINE Signal Check
        //         ENGINE_FLAG = 0x00000001 = 2^0 --> SW0
        if (event_SwitchIOs & ENGINE_FLAG)
        {
            // led_red = led_red | LED_RED_0;
            Signal_Engine = on;
        }
        else
        {
            Signal_Engine = off;
        }

        //  |- GEAR Signal Check
        //         ENGINE_FLAG = 0x00000002 = 2^1 --> SW1
        if (event_SwitchIOs & TOP_GEAR_FLAG)
        {
            // If TOP_GEAR is ON --> Turn ON the LED_RED_1
            led_red = led_red | LED_RED_1;
            Signal_TopGear = on;
        }
        else
        {
            Signal_TopGear = off;
        }

        // Check total SwitchIOs config in order to detect the created
        // extra loads through SwitchIOs [SW9, SW0]
        // extra load checking Mask:
        //         SW9 SW8 SW7 SW6 SW5 SW4 SW3 SW2 SW1 SW0
        // 0x3F0 =  1   1   1   1   1   1   0   0   0   0  = 63 = 2^6 - 1
        Signal_ExtraLoadAdjustIO = event_SwitchIOs & extraload_check_mask;

        // Turn ON the extra Tasks LEDs
        led_red = led_red | Signal_ExtraLoadAdjustIO;

        // Convert the binary dummy extra tasks to Decimal base
        // Then, the range [0, 63] can be adjusted by the 6 Keys
        Signal_ExtraLoadAdjustIO = Signal_ExtraLoadAdjustIO >> 4;

        // Sent the captured Signals to their Mboxes
        //      |- Signal_Engine   --> Mbox_Engine
        //      |- Signal_TopGear  --> Mbox_Gear
        //      |- Signal_ExtraLoadAdjustIO --> Mbox_ExtraLoadCreation
		err = OSMboxPost(Mbox_Engine, (void *) &Signal_Engine);
 		err = OSMboxPost(Mbox_Gear, (void *) &Signal_TopGear);
        err = OSMboxPost(Mbox_ExtraLoadCreation, (void *) &Signal_ExtraLoadAdjustIO);
    }
}

//         |- KeyIOTask() Body
void KeyIOTask(void* pdata)
{
    INT8U err;
    enum active Signal_CruiseControl = off;
    enum active Signal_GasPedal = off;
    enum active Signal_BrakePedal = off;
    int event_KeyIOs;

    while (1)
    {
        OSSemPend(Sem_KeyIO, 0, &err);
        // KeyIOs scanning begin
        // switches_pressed() return the scanned KeyIOs config in bit-wise
        event_KeyIOs = buttons_pressed();

        /* Bit Operation Description
         * Use Bit operation to check each KeyIOs' status
         * e.g.: CRUISE CONTROL Check
         *      CRUISE_CONTROL_FLAG = 0x02 = 2^1
         *      Use CRUISE_CONTROL_FLAG as a Mask
         *      Do AND opt. on current KeyIOs config 'event_KeyIOs'
         *      Other bits related to other IOs will be filtered out
         *      If the bit represent the CRUISE_CONTROL ON is 1
         *      then signal Signal_CruiseControl on, otherwise, signal engine OFF
         */
        //      |- Cruise Control signal check
        //         CRUISE_CONTROL_FLAG = 0x02 = 2^1 --> Key1
        if (event_KeyIOs & CRUISE_CONTROL_FLAG)
        {
            Signal_CruiseControl = on;
        }
        else
        {
            Signal_CruiseControl = off;
        }

        //      |- Brake Pedal signal check
        //         BRAKE_PEDAL_FLAG = 0x04 = 2^2 --> Key2
        if (event_KeyIOs & BRAKE_PEDAL_FLAG)
        {
            led_green = led_green | LED_GREEN_4;
            Signal_BrakePedal = on;
        }
        else
        {
            Signal_BrakePedal = off;
        }

        //      |- Gas Pedal signal check
        //         GAS_PEDAL_FLAG = 0x08 = 2^3 --> Key3
        if (event_KeyIOs & GAS_PEDAL_FLAG)
        {
            // led_green = led_green | LED_GREEN_6;
            Signal_GasPedal = on;
        }
        else
        {
            Signal_GasPedal = off;
        }

        // Sent the captured Signals to their Mboxes
        //      |- Signal_CruiseControl --> Mbox_Cruise
        //      |- Signal_BrakePedal   --> Mbox_Brake
        //      |- Signal_GasPedal  --> Mbox_Gas
        err = OSMboxPost(Mbox_Cruise, (void *) &Signal_CruiseControl);
        err = OSMboxPost(Mbox_Brake, (void *) &Signal_BrakePedal);
        err = OSMboxPost(Mbox_Gas, (void *) &Signal_GasPedal);
    }
}

//         |- OverloadDetectionTask() Body
void OverloadDetectionTask(void* pdata)
{
    // This OverloadDetection Task has the lowest task priority
    // within the entire program with 300 ms as period
    // The working mechanism of this function is as below:
    //      | if: it can be executed once within the HYPER_PERIOD = 300 MS
    //          |--> OK signal will be sent to the Mbox_Overload
    //          |--> Thereby, the Watchdog can pend it from the Mbox_Overload
    //      | else if: it cannot be executed once within the HYPER_PERIOD = 300 MS
    //          |--> Which means, the system utilizantion reached 100% = 300 / 300
    //          |--> because the system has no time to execute this detection
    //          |--> Thereby, the Watchdog cannot post the OK signal from the
    //          |--> Mbox_Overload. Then the dog will yell.
    INT8U err;
    int signal_ok = 1;
    while(1)
    {
        OSSemPend(Sem_OverloadDetection, 0, &err);
        err = OSMboxPost(Mbox_Overload, (void *)signal_ok);
	}
}

//         |- WatchdogTask() Body
void WatchdogTask(void* pdata)
{
    // Wait for the OK signal from Mbox_Overload
    // with 300 ms time-out
    // if the Watchdog cannot receive the OK signal after 300 ms
    // shoot the Overload alarm
    INT8U err;
    void *msg;
    while(1)
    {
        msg = OSMboxPend(Mbox_Overload, 300, &err);
        msg = OSMboxAccept(Mbox_Overload);      // just to clear it.
        if(err == OS_ERR_TIMEOUT)
        {
            printf("\n");
            printf("--x--x--x--> Watchdog: Overload is detected!\n");
            printf("\n");
        }
	}
}

//         |- ExtraLoadTask() Body???
void ExtraLoadTask(void* pdata)
{
    INT8U err;
    void *msg;

	// INT16U overload = 0;
    INT16U adjusted_extra_load = 0;
	INT16U overload_percentage = 0;
    INT16U waiting_factor = 3;  // H = 300
    INT16U adjusted_step = 2;   // The utilization shall be adjustable in 2% steps.
    INT32U left_tick;

	while(1)
    {
		OSSemPend(Sem_ExtraLoad, 0, &err);
		msg = OSMboxPend(Mbox_ExtraLoadCreation, 0, &err);
		adjusted_extra_load = *((INT16U*) msg);

		overload_percentage = adjusted_step * adjusted_extra_load;

        if (overload_percentage > 100)
        {
            overload_percentage = 100;
        }

		printf("--=--=--=--=--=--> Utilization Rate adjusted by Extra Load SwitchIOs [SW9, SW4]: %d %% \n", overload_percentage);

	    left_tick = OSTimeGet();

        // dummy overloading waiting time
		while((OSTimeGet() - left_tick) < (waiting_factor * overload_percentage));
	}
}


/*
 * The task 'StartTask' creates all other tasks kernel objects and
 * deletes itself afterwards.
 */
 //         |- StartTask() Body
void StartTask(void* pdata)
{
    INT8U err;
    void* context;

    static alt_alarm alarm;     /* Is needed for timer ISR function */

    /* Base resolution for SW timer : HW_TIMER_PERIOD ms */
    delay = alt_ticks_per_second() * HW_TIMER_PERIOD / 1000;
    printf("delay in ticks %d\n", delay);

    /*
    * Create Hardware Timer with a period of 'delay'
    */
    if (alt_alarm_start(&alarm, delay, alarm_handler, context) < 0)
    {
        printf("No system clock available!n");
    }

    /*
    * Create and start Software Timer Tools
    */
    BOOLEAN timer_status;
    //      |- Instantiate Semaphores
    Sem_Vehicle = OSSemCreate(0);
    Sem_Control = OSSemCreate(0);
    Sem_SwitchIO = OSSemCreate(0);
    Sem_KeyIO = OSSemCreate(0);
    Sem_OverloadDetection = OSSemCreate(0);
    Sem_ExtraLoad = OSSemCreate(0);

    //      |- Instantiate Periodic Timers
    Timer_Vehicle = OSTmrCreate(
        0,
        // VEHICLE_PERIOD / 10
        VEHICLE_PERIOD / HW_TIMER_PERIOD,
        OS_TMR_OPT_PERIODIC,
        Timer_Vehicle_CallbackSemPost,
        NULL,
        NULL,
        &err
    );

    Timer_Control = OSTmrCreate(
        0,
        CONTROL_PERIOD / HW_TIMER_PERIOD,
        OS_TMR_OPT_PERIODIC,
        Timer_Control_CallbackSemPost,
        NULL,
        NULL,
        &err
    );

    Timer_SwitchIO = OSTmrCreate(
        0,
        SWITCHIO_PERIOD / HW_TIMER_PERIOD,
        OS_TMR_OPT_PERIODIC,
        Timer_SwitchIO_CallbackSemPost,
        NULL,
        NULL,
        &err
    );

    Timer_KeyIO = OSTmrCreate(
        0,
        KEYIO_PERIOD / HW_TIMER_PERIOD,
        OS_TMR_OPT_PERIODIC,
        Timer_KeyIO_CallbackSemPost,
        NULL,
        NULL,
        &err
    );

    Timer_OverloadDetection = OSTmrCreate(
        0,
        HYPER_PERIOD / HW_TIMER_PERIOD,
        OS_TMR_OPT_PERIODIC,
        Timer_OverloadDetection_CallbackSemPost,
        NULL,
        NULL,
        &err
    );

    Timer_ExtraLoad = OSTmrCreate(
        0,
        HYPER_PERIOD / HW_TIMER_PERIOD,
        OS_TMR_OPT_PERIODIC,
        Timer_ExtraLoad_CallbackSemPost,
        NULL,
        NULL,
        &err
    );

    //      |- Start Timers
    timer_status = OSTmrStart(Timer_Vehicle, &err);
    timer_status = OSTmrStart(Timer_Control, &err);
    timer_status = OSTmrStart(Timer_SwitchIO, &err);
    timer_status = OSTmrStart(Timer_KeyIO, &err);
    timer_status = OSTmrStart(Timer_OverloadDetection, &err);
    timer_status = OSTmrStart(Timer_ExtraLoad, &err);

    /*
    * Creation of Kernel Objects
    */
    // Mailboxes
    Mbox_Throttle = OSMboxCreate((void*) 0);    /* Empty Mailbox - Throttle */
    Mbox_Velocity = OSMboxCreate((void*) 0);    /* Empty Mailbox - Velocity */
    // Mbox_Brake = OSMboxCreate((void*) 1);
    // Mbox_Engine = OSMboxCreate((void*) 1);
    Mbox_Brake = OSMboxCreate((void*) 0);
    Mbox_Engine = OSMboxCreate((void*) 0);

    Mbox_Gas = OSMboxCreate((void*) 0);
    Mbox_Gear = OSMboxCreate((void*) 0);
    Mbox_Cruise = OSMboxCreate((void*) 0);
    Mbox_Overload = OSMboxCreate((void*) 0);
    Mbox_ExtraLoadCreation = OSMboxCreate((void*) 0);

    /*
    * Create statistics task
    */
    OSStatInit();

    /*
    * Creating Tasks in the system
    */
    //      |- ControlTask()
    err = OSTaskCreateExt(
        ControlTask,                            // Pointer to task code
        NULL,                                   // Pointer to argument that is passed to task
        &ControlTask_Stack[TASK_STACKSIZE-1],   // Pointer to top of task stack
        CONTROLTASK_PRIO,
        CONTROLTASK_PRIO,
        (void *)&ControlTask_Stack[0],
        TASK_STACKSIZE,
        (void *) 0,
        OS_TASK_OPT_STK_CHK
    );

    //      |- VehicleTask()
    err = OSTaskCreateExt(
        VehicleTask,                            // Pointer to task code
        NULL,                                   // Pointer to argument that is passed to task
        &VehicleTask_Stack[TASK_STACKSIZE-1],   // Pointer to top of task stack
        VEHICLETASK_PRIO,
        VEHICLETASK_PRIO,
        (void *)&VehicleTask_Stack[0],
        TASK_STACKSIZE,
        (void *) 0,
        OS_TASK_OPT_STK_CHK
    );

    //      |- SwitchIOTask()
    err = OSTaskCreateExt(
        SwitchIOTask,                            // Pointer to task code
        NULL,                                    // Pointer to argument that is passed to task
        &SwitchIOTask_Stack[TASK_STACKSIZE-1],   // Pointer to top of task stack
        SWITCHIOTASK_PRIO,
        SWITCHIOTASK_PRIO,
        (void *)&SwitchIOTask_Stack[0],
        TASK_STACKSIZE,
        (void *) 0,
        OS_TASK_OPT_STK_CHK
    );

    //      |- KeyIOTask()
    err = OSTaskCreateExt(
        KeyIOTask,                              // Pointer to task code
        NULL,                                   // Pointer to argument that is passed to task
        &KeyIOTask_Stack[TASK_STACKSIZE-1],     // Pointer to top of task stack
        KEYIOTASK_PRIO,
        KEYIOTASK_PRIO,
        (void *)&KeyIOTask_Stack[0],
        TASK_STACKSIZE,
        (void *) 0,
        OS_TASK_OPT_STK_CHK
    );

    //      |- OverloadDetectionTask()
    err = OSTaskCreateExt(
        OverloadDetectionTask,                              // Pointer to task code
        NULL,                                               // Pointer to argument that is passed to task
        &OverloadDetectionTask_Stack[TASK_STACKSIZE-1],     // Pointer to top of task stack
        OVERLOADDETECTIONTASK_PRIO,
        OVERLOADDETECTIONTASK_PRIO,
        (void *)&OverloadDetectionTask_Stack[0],
        TASK_STACKSIZE,
        (void *) 0,
        OS_TASK_OPT_STK_CHK
    );

    //      |- WatchdogTask()
    err = OSTaskCreateExt(
        WatchdogTask,                            // Pointer to task code
        NULL,                                    // Pointer to argument that is passed to task
        &WatchdogTask_Stack[TASK_STACKSIZE-1],   // Pointer to top of task stack
        WATCHDOGTASK_PRIO,
        WATCHDOGTASK_PRIO,
        (void *)&WatchdogTask_Stack[0],
        TASK_STACKSIZE,
        (void *) 0,
        OS_TASK_OPT_STK_CHK
    );

    //      |- ExtraLoadTask()
    err = OSTaskCreateExt(
        ExtraLoadTask,                            // Pointer to task code
        NULL,                                     // Pointer to argument that is passed to task
        &ExtraLoadTask_Stack[TASK_STACKSIZE-1],   // Pointer to top of task stack
        EXTRALOADTASK_PRIO,
        EXTRALOADTASK_PRIO,
        (void *)&ExtraLoadTask_Stack[0],
        TASK_STACKSIZE,
        (void *) 0,
        OS_TASK_OPT_STK_CHK
    );

    printf("All Tasks and Kernel Objects generated!\n");

    /* Task deletes itself */

    OSTaskDel(OS_PRIO_SELF);
}

/*
 *
 * The function 'main' creates only a single task 'StartTask' and starts
 * the OS. All other tasks are started from the task 'StartTask'.
 *
 */

int main(void) {
    printf("--=--=--=-- Lab: Cruise Control --=--=--=--\n");

    OSTaskCreateExt(
        StartTask,                                      // Pointer to task code
        NULL,                                           // Pointer to argument that is
                                                        // passed to task
        (void *)&StartTask_Stack[TASK_STACKSIZE-1],     // Pointer to top
                                                        // of task stack
        STARTTASK_PRIO,
        STARTTASK_PRIO,
        (void *)&StartTask_Stack[0],
        TASK_STACKSIZE,
        (void *) 0,
        OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR
    );

    OSStart();

    return 0;
}
