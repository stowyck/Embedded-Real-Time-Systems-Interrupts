/**************************************************************************//**
 *
 * @file        Main.c
 * @brief       Entry point for the program
 * @author      Geoffrey Daniels, Dimitris Agrafiotis
 * @author		Sam Walder, Jeremy Dalton 12/2012
 * @version     1.0
 * @date        19 July. 2012
 *
 * Copyright(C) 2012, University of Bristol
 * All rights reserved.
 *
******************************************************************************/


/******************************************************************************
 * Includes
 *****************************************************************************/
// Standard C library
#include <stdio.h>
#include <string.h>
//#include <stdbool.h> //for bool type
// LPC17xx definitions for CMSIS
#include "LPC17xx.h"

// LPC17xx drivers (that use CMSIS)
#include "LPC17xx_Types.h"
#include "LPC17xx_PinSelect.h"
#include "LPC17xx_GPIO.h"
#include "LPC17xx_SSP.h"
#include "LPC17xx_I2C.h"
#include "LPC17xx_ADC.h"
#include "LPC17xx_UART.h"
#include "LPC17xx_Timer.h"
#include "LPC17xx_SysTick.h"
#include "LPC17xx_LED2.h"

// Baseboard drivers (that use LPC17xx drivers)
#include "dfrobot.h"
#include "OLED.h"
#include "Buttons.h"
#include "RotarySwitch.h"
#include "SevenSegment.h"
#include "Tune.h"
#include "pca9532.h"
#include "joystick.h"
#include "new_string.h"


//Definitions for timers
#define SBIT_TIMER0  1
#define SBIT_TIMER1  2
#define SBIT_TIMER2	 22
#define SBIT_TIMER3	 23
#define SBIT_MR0I    0
#define SBIT_MR0R    1
#define SBIT_MR0S    2
#define SBIT_CNTEN   0
#define PCLK_TIMER0  2
#define PCLK_TIMER1  4



/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/
// PCADC / PCAD
#define ADC_POWERON (1 << 12) //12 bit shift left of 01
#define PCLK_ADC 24
#define PCLK_ADC_MASK (3 << 24) //24bit shift left of 11

// AD0.0 - P0.23, PINSEL1 [15:14] = 01
#define SELECT_ADC0 (0x1<<14)

// ADOCR constants
#define START_ADC (1<<24)
#define OPERATIONAL_ADC (1 << 21)
#define SEL_AD0 (1 <<0)
#define ADC_DONE_BIT	(1 << 31)

/******************************************************************************
 * Global variables
 *****************************************************************************/
uint8_t Gear = 1;		//setting gear from 1 to 0

uint8_t Stop = 0;

uint16_t ADCval;

	//specifying mode of operation
	typedef enum  {TUNE1, TUNE2, DRIVE} mode_type;

	volatile mode_type MODE = TUNE1;

	char* mode_name[] = {"TUNE1", "TUNE2", "DRIVE"};

	volatile uint16_t i = 3; //accumulator for mode of operation

	//specifying state of tune
	typedef enum  {PLAY, PAUSE, STOP} song_type;

	extern volatile song_type STATE = STOP;
	char* play_name[] = {" PLAY","PAUSE"," STOP"};
									//bool will be used in the index change function
	typedef enum {false, true} bool; //moved line to Tune.h file
	volatile int tPin = 0;//false; //pause to play transition pin type defined in Tune.h

	uint16_t pitchState[] ={ //needed for LED arrays function takes in 16bits then sets array high or low, function mask is always 0xffff
			0b0000000000000000,
			0b1000000000000000,
			0b1100000000000000,
			0b1110000000000000,
			0b1111000000000000,
			0b1111100000000000,
			0b1111110000000000,
			0b1111111000000000,
			0b1111111100000000,
	};

	uint16_t tempoState[] ={	
			0b0000000000000000,
			0b0000000000000001,
			0b0000000000000011,
			0b0000000000000111,
			0b0000000000001111,
			0b0000000000011111,
			0b0000000000111111,
			0b0000000001111111,
			0b0000000011111111,

	};


	//both songs can use the same tempo array with static increment to retain tempo value

	//set j index to value of 1 in above array, found in Tune.c
	static volatile uint8_t j = 3;
	static volatile uint8_t n = 4;

	uint8_t index_change(bool incdec){
		//only initialises once refers to value of 1 in array
		pca9532_setLeds(pitchState[n]|tempoState[j], 0xffff);
		if(incdec == false){
			if(j>0){//decrement if argument is zero
				j--;
				pca9532_setLeds(pitchState[n]|tempoState[j], 0xffff);
			}
		}
		else{
			if(j<7){//0 to 7, array size of 8
				j++;
				pca9532_setLeds(pitchState[n]|tempoState[j], 0xffff);
			}

		}

		return j;

	}

	uint8_t index_change2(bool incdec){
		//only initialises once refers to value of 1 in array
		pca9532_setLeds(pitchState[n]|tempoState[j], 0xffff);
		if(incdec == false){
			if(n>0){//decrement if argument is zero
				n--;
				pca9532_setLeds(pitchState[n]|tempoState[j], 0xffff);

			}
		}
		else{
			if(n<7){//0 to 7, array size of 8
				n++;
				pca9532_setLeds(pitchState[n]|tempoState[j], 0xffff);
			}

		}

		return n;

	}

	//cases for the drive mode...............
	typedef enum {FORWARD, BACKWARD, STOPPED, LEFT, RIGHT} drive_type;
	volatile drive_type DSTATE = STOPPED;
	char* drive_name[] = {"forward","reverse","stopped","left   ","right  "};
	
	


	static volatile uint32_t wheelDistance = 0;


	volatile uint32_t beginNum; 			// initial value of interrupt timer
	volatile uint32_t endNum; 				// end value of interrupt timer
	volatile uint32_t counterIncrement = 0; //1ms interrupt increments
	volatile uint32_t runTimeCPU;			// available CPU time
	
	volatile uint32_t intCount; 			// saves duration of interrupt handling

	volatile float oneSecond = 0;
	char string[2] = "00";
	volatile char string2[] = "0";




/******************************************************************************
 * Local Functions
 *****************************************************************************/
 
 /******************************************************************************
 * Description:
 *    Simple delaying function. Not good. Blocking code.
 *****************************************************************************/


/*static void DelayMS(int Length)		//maybe need to get rid of all occurrences of delays
{
   volatile int Delay;
   volatile int D;
   for (Delay=0; Delay<Length*3000; Delay++)
   {
	   D = Delay;
   }
}
*/


/******************************************************************************
 * Description:
 *    Flash the LED on the LPC1769 as a heart beat
 *****************************************************************************/
void SysTick_Handler(void)    {
	
	/*if((counterIncrement++ % 1000) == 0){//every 1 second display CPU usage
			//here display use calculate percentage function using runTimeCPU
			runTimeCPU = 0;
	}*/
	// variable descriptions in declaration

		oneSecond++;

	if (oneSecond > 500){											// make the LED switch at every 1s increment
			runTimeCPU = 100*(intCount/oneSecond);
			// this calculates what proportion of time is spent on interrupts in the last 1s
			// Display calculated value as a string on OLED
			floatToString(runTimeCPU,string,3);
			WriteOLEDString(string, 3, 9);
			oneSecond = 0;
			intCount =0;
	}
	
}

void Init_SysTick1000ms(void) { if (SysTick_Config(SystemCoreClock / 1))    { while(1) { ; } } }
void Init_SysTick100ms(void)  { if (SysTick_Config(SystemCoreClock / 10))   { while(1) { ; } } }
void Init_SysTick10ms(void)   { if (SysTick_Config(SystemCoreClock / 100))  { while(1) { ; } } }
void Init_SysTick1ms(void)    { if (SysTick_Config(SystemCoreClock / 1000)) { while(1) { ; } } }



/******************************************************************************
* Description:
*    Function for prescalar
*****************************************************************************/

//extern unsigned int SystemCoreClock;


unsigned int getPrescalarForUs(uint8_t timerPclkBit)	//prescalor for microseconds
{
    unsigned int pclk,prescalarForUs;
    pclk = (LPC_SC->PCLKSEL0 >> timerPclkBit) & 0x03;  /* get the pclk info for required timer */

    switch ( pclk )                                    /* Decode the bits to determine the pclk*/
    {
    case 0x00:
        pclk = SystemCoreClock/4;
        break;
    case 0x01:
        pclk = SystemCoreClock;
        break;
    case 0x02:
        pclk = SystemCoreClock/2;
        break;
    case 0x03:
        pclk = SystemCoreClock/8;
        break;
    default:
        pclk = SystemCoreClock/4;
        break;
    }

    prescalarForUs = pclk/1000000 - 1;                  /* Prescalar for 1us (1000000Counts/sec) */

    return prescalarForUs;
}






/******************************************************************************
 * Description: Initialise the SPI hardware
 *    
 *****************************************************************************/
//Serial Peripheral Interface Bus
static void Init_SSP(void)
{
	SSP_CFG_Type SSP_Config;
	PINSEL_CFG_Type PinConfig;

	// Initialize SPI pin connect
	// P0.7 - SCK; P0.8 - MISO; P0.9 - MOSI; P2.2 - SSEL - used as GPIO
	PinConfig.Funcnum = 2;
	PinConfig.OpenDrain = 0;
	PinConfig.Pinmode = 0;
	PinConfig.Portnum = 0;
	PinConfig.Pinnum = 7;
	PINSEL_ConfigPin(&PinConfig);
	PinConfig.Pinnum = 8;
	PINSEL_ConfigPin(&PinConfig);
	PinConfig.Pinnum = 9;
	PINSEL_ConfigPin(&PinConfig);
	PinConfig.Funcnum = 0;
	PinConfig.Portnum = 2;
	PinConfig.Pinnum = 2;
	PINSEL_ConfigPin(&PinConfig);
	SSP_ConfigStructInit(&SSP_Config);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_Config);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);
}

/******************************************************************************
 * Description: Initialise the I2C hardware
 *    
 *****************************************************************************/
static void Init_I2C(void)
{
	PINSEL_CFG_Type PinConfig;

	/* Initialize I2C2 pin connect */
	PinConfig.Funcnum = 2;
	PinConfig.Pinnum = 10;
	PinConfig.Portnum = 0;
	PINSEL_ConfigPin(&PinConfig);
	PinConfig.Pinnum = 11;
	PINSEL_ConfigPin(&PinConfig);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	// Enable I2C1 operation
 	I2C_Cmd(LPC_I2C2, I2C_MASTER_MODE, ENABLE);
}

/******************************************************************************
 * Description: Configure an ADC channel
 *    
 *****************************************************************************/
static void Init_ADC(void) //ANALOGUE to digital converter not initialised
{
	/*
	PINSEL_CFG_Type PinConfig;
	PinConfig.Funcnum = 1;
	PinConfig.OpenDrain = 0;
	PinConfig.Pinmode = 0;
	PinConfig.Pinnum = 23;			// Channel AD0.0
	PinConfig.Portnum = 0;
    PINSEL_ConfigPin(&PinConfig);


	ADC_Init(LPC_ADC, 200000);
	ADC_ChannelCmd(LPC_ADC,0,ENABLE);
	ADC_IntConfig(LPC_ADC,ADC_ADINTEN0,SET);
	ADC_BurstCmd(LPC_ADC,1);
	ADC_StartCmd(LPC_ADC,ADC_START_CONTINUOUS);
	*/
}

/******************************************************************************
 * Description: Run some init functions
 *    
 *****************************************************************************/
void Init(void)	//initialising hardware functions defined above
{
	// LPC1769
	LED2_Init();
	Init_SysTick100ms();
	Init_SSP();//
	Init_I2C();
	//Init_ADC();
	LED2_On();

	// Baseboard
	Tune_Init();
	OLED_Init();
	RotarySwitch_Init();
	SevenSegment_Init();
	pca9532_init();//on board chip internal oscillator LED dimmer etc
	joystick_init(); //joystick is initiated here

	// Extra Hardware
	DFR_RobotInit(); //motor driver etc


	//timer0 and timer 1
	LPC_SC->PCONP |= (1<<SBIT_TIMER0) | (1<<SBIT_TIMER1) | (1<<SBIT_TIMER2) | (1<<SBIT_TIMER3);		 	 	//powering up timer0 snd time1

	LPC_TIM0->MCR  = (1<<SBIT_MR0I) | (1<<SBIT_MR0R) | (1<<SBIT_MR0S);  //enable interrupts on match register0 value, reset the timer counter TC on match register0, stop timer on match register0
	LPC_TIM1->MCR  = (1<<SBIT_MR0I) | (1<<SBIT_MR0R) | (1<<SBIT_MR0S);	//same thing //However may need to change so that we don't force it to call itself see online example
	LPC_TIM2->MCR  = (1<<SBIT_MR0I) | (1<<SBIT_MR0R) | (1<<SBIT_MR0S);
	LPC_TIM3->MCR  = (1<<SBIT_MR0I) | (1<<SBIT_MR0R) | (1<<SBIT_MR0S);

	//LPC_TIM0->PR   = getPrescalarForUs(PCLK_TIMER0);                    /* Prescalar for 1us */
	//LPC_TIM1->PR   = getPrescalarForUs(PCLK_TIMER1);					  /* Prescalar for 1us */



	//Last enabling of all required external interupts
					//	 B	  A	  Cntr right down	SW3
	GPIO_IntCmd( 0, (1<<25|1<<24|1<<17|1<<16|1<<15|1<<4), 0 );//all set to rising edge
								//left up
	GPIO_IntCmd( 2, (1<<12|1<<11|1<<4|1<<3), 0 );//all set to rising edge

	NVIC_EnableIRQ(TIMER0_IRQn);                          				// Enable Timer Interrupts
	NVIC_EnableIRQ(TIMER1_IRQn);
	NVIC_EnableIRQ(TIMER2_IRQn);
	NVIC_EnableIRQ(TIMER3_IRQn);

	// Enable GPIO Interrupts
	NVIC_EnableIRQ(EINT3_IRQn);	//EINT3_IRQn is the interrupt ID, for external interrupt 3;
								//can use the name i.e EINT3_IRQn or the corresponding number which is 21 from lpc17xx.h
								//Any GPIO pin used for external interrupts will be using external interrupt channel 3 (EINT3).
								//Four dedicated pins (P2.10, P2.11, P2.12 and P2.13) that act as EINT0, EINT1, EINT2, and EINT3, respectively.
								//possible that all pins that need to be used are mapped back to P2.13 which enables an external interrupt
								//refer to lpc17xx_pinsel.c and use static void set_PinFunc ( uint8_t portnum, uint8_t pinnum, uint8_t funcnum); to set function of any new pins/ports

	//we should set priority for the timers 
	NVIC_SetPriority(EINT3_IRQn,3);
	NVIC_SetPriority(TIMER3_IRQn,4);
	NVIC_SetPriority(TIMER2_IRQn,5);
	NVIC_SetPriority(TIMER0_IRQn,6);
	NVIC_SetPriority(TIMER1_IRQn,7);
}

/******************************************************************************
 * Description: Main program entry point
 *    
 *****************************************************************************/
int main (void)
{
	// Globally disable interrupts
	__disable_irq();

	// Initialise
	Init();		//initialised hardware

	// Globally enable interrupts
	__enable_irq(); //ARM cortex interrupts

  	// Initialise OLED contents
	OLED_ClearScreen(OLED_COLOR_WHITE);
	WriteOLEDString((uint8_t*)"Mode:", 0, 0); //writing mode of operation //starting position for LED screen this is what it writes to the screen
	WriteOLEDString((uint8_t*)mode_name[MODE], 0, 5);
	WriteOLEDString((uint8_t*)play_name[STATE], 0, 10);
	WriteOLEDString((uint8_t*)"Driving ", 2, 0);	//use the state of drive
	WriteOLEDString((uint8_t*)drive_name[DSTATE], 2, 8);
	WriteOLEDString((uint8_t*)"CPU is ", 3, 0);
	WriteOLEDString((uint8_t*)"Travelled ", 4, 0);
	floatToString(wheelDistance,string2,3);
	WriteOLEDString(string2, 4, 10);

	// Set the seven segment to the current 'gear'
	Gear = DFR_GetGear();							//Gear unsigned 8 bit integer value 		just returns DFR_Gear a global variable
	SevenSegment_SetCharacter('0' + Gear, FALSE);//SevenSegment_SetCharacter('0' + sevenSeg, FALSE);//SevenSegment_SetCharacter('0' + sevenSeg, FALSE); //SevenSegment_SetCharacter('5', FALSE); //

	//LPC_GPIO0->FIOMASK = 0x0; //pin mask probably not needed.
	//LPC_GPIO1->FIOSET |=

	// Main program loop
	while (1)
	{

			//main loop is intended to be empty
	
	
	}//main while loop//////

}//main program









/*******************************************************************************************************************************************************************************************************
 *							Interrupt Service Routines
 *
 ******************************************************************************************************************************************************************************************************/
void EINT3_IRQHandler (void)
{


	beginNum =  SYSTICK_GetCurrentValue();
		
	//__disable_irq();
	//if  joystick centre is being pressed increment the mode of operation //rising edge
	if ((((LPC_GPIOINT->IO0IntStatR) >> 17)& 0x1) == ENABLE) //may need to read flag directly //change to read directly from
	{
		MODE = (mode_type)(++i % 3); //using the pre-increment and type casting
		WriteOLEDString((uint8_t*)mode_name[MODE], 0, 5);//update mode of operation to LCD screen
		//possibly going to get rid of so can play song whilst driving
		//STATE = STOP;//whenever switching between modes change the state of the song to stop
		tPin = 0;//when switching between modes set from pause to play pin to zero
		if(MODE == DRIVE){WriteOLEDString((uint8_t*)"     ", 0, 10); STATE = STATE;}
	}
	//GPIO_ClearInt(0,1 << 17);//here we clear the flag for the centre joy stick press (no longer being pressed)...


	
	if((((LPC_GPIOINT->IO0IntStatR) >> 4)& 0x1) == ENABLE) //must use button reads for hold functionality....//if the button has been pressed; if the button has not been pressed we do not enter
	{
			switch(STATE){
			case PAUSE: STATE = PLAY;//going from pause to play
						 tPin = 1;//transition flag set to 1
							break;
			case PLAY: STATE = PAUSE;
						tPin = 0;
							break;
			case STOP: STATE = PLAY;
						tPin = 0;
							break;
			default:
				break;
						}

	}
	if ((MODE == TUNE1 || MODE == TUNE2) && (((((LPC_GPIOINT->IO2IntStatR) >> 4)& 0x1) == ENABLE) || ((((LPC_GPIOINT->IO0IntStatR) >> 16)& 0x1) == ENABLE))){//cannot drive and stop song at the same time, however we can pause the song 
			tPin = 0;
			STATE = STOP;
		
	}
	//GPIO_ClearInt(0,1 << 4);//set flags back to zero
	//maybe do both button clears at the end of ISR
	//__enable_irq();


	//encoder

	if ((((LPC_GPIOINT->IO2IntStatR) >> 11)& 0x1) == ENABLE){
				// Increase left wheel counter
				DFR_IncLeftWheelCount();

				// Compare the current wheel counter value with destination value
				if (DFR_GetLeftWheelCount() == DFR_GetLeftWheelDestination()) {

					// End movement
					DFR_DriveStop();
					DSTATE=STOPPED;
					// Clear the wheel counters
					wheelDistance +=  DFR_GetRightWheelCount();
					floatToString(wheelDistance,string2,3);
					WriteOLEDString(string2, 4, 10);
					DFR_ClearWheelCounts();
					DFR_SetLeftWheelDestination(0);
				}
			}	// Right Encoder
			else if ((((LPC_GPIOINT->IO2IntStatR) >> 12)& 0x1) == ENABLE){
			// Increase right wheel counter
			DFR_IncRightWheelCount();

				//Compare the current wheel counter value with destination value.
				if (DFR_GetRightWheelCount() == DFR_GetRightWheelDestination()) {

					// End movement.
					DFR_DriveStop();
					DSTATE=STOPPED;
					// Clear the wheel counters.
					wheelDistance +=  DFR_GetRightWheelCount();
					floatToString(wheelDistance,string2,3);
					WriteOLEDString(string2, 4, 10);
					DFR_ClearWheelCounts();
					DFR_SetRightWheelDestination(0);

				}
			}


		//rotary switch for gear
			if(((((LPC_GPIOINT->IO0IntStatR) >> 24)& 0x1) == ENABLE)&&(MODE == DRIVE)){//if rotary stick CLOCKWISE flag is set
				//increment
				DFR_IncGear();//max gear is 4 min gear is 1
				Gear = DFR_GetGear();
				SevenSegment_SetCharacter('0' + Gear, FALSE);
				WriteOLEDString((uint8_t*)"Increasing gear", 8, 0);
				WriteOLEDString((uint8_t*)"               ", 8, 0);
				
			}
			else if(((((LPC_GPIOINT->IO0IntStatR) >> 25)& 0x1) == ENABLE)&&(MODE == DRIVE)){//if if rotary stick ANTICLOCKWISE flag
				//decrement
				DFR_DecGear();
				Gear = DFR_GetGear();
				SevenSegment_SetCharacter('0' + Gear, FALSE);
				WriteOLEDString((uint8_t*)"Decreasing gear", 8, 0);
				WriteOLEDString((uint8_t*)"               ", 8, 0);
			}
			//clear flags
			//GPIO_ClearInt(0, (1<<25 | 1<<24));







/****************************************************************************************************************
 *
 *											MODE CASE AMD SWITCH
 *
 ****************************************************************************************************************/
switch(MODE){




/****************************************************************************************************************
 *													tune1
 ****************************************************************************************************************/
	case TUNE1:	//here we play/pause/stop song 1
	
	//here joystick to manipulate tempo

			if((((LPC_GPIOINT->IO2IntStatR) >> 3)& 0x1) == ENABLE){//if Joystick UP flag is set	may need to use a pin read
				//increment
				Tune_SetTempo(index_change(1));
			}
			else if((((LPC_GPIOINT->IO0IntStatR) >> 15)& 0x1) == ENABLE){//if Joystick DOWN flag
				//decrement
				Tune_SetTempo(index_change(0));
			}
			//clear flags
			//GPIO_ClearInt(0, 1<<15); GPIO_ClearInt(2, 1<<3);

	//rotary switch for pitch
			if((((LPC_GPIOINT->IO0IntStatR) >> 24)& 0x1) == ENABLE){//if rotary stick CLOCKWISE flag is set	may need to use
				//increment
				Tune_SetPitch(index_change2(1));
			}
			else if((((LPC_GPIOINT->IO0IntStatR) >> 25)& 0x1) == ENABLE){//if if rotary stick ANTICLOCKWISE flag
				//decrement
				Tune_SetPitch(index_change2(0));
			}
			//clear flags
			//GPIO_ClearInt(0, (1<<25 | 1<<24));



		switch(STATE){//nested switch and case
			case STOP:  //STOP stop song
					WriteOLEDString((uint8_t*)play_name[STATE], 0, 10);
					Tune_StopSong();//pointer is set to NULL

				break;
			case PLAY:  //PLAY play song
					WriteOLEDString((uint8_t*)play_name[STATE], 0, 10);	 //encountering problem going from pause to play and the value of tPin when the song has ended
					//if tune is playing do not enter again
					Tune_PlaySong(Tune_SampleSongs[0], tPin);//defined in tune.h main.c and tune.c have access to this song Tune_SampleSongs
												 	 	 	 //sends the address of the first character in first char string
															 //set transition flag back to zero after playing song
					STATE = STOP;//if tune is not longer playing change the state to STOP
					//WriteOLEDString((uint8_t*)play_name[STATE], 0, 10);
				break;
			case PAUSE: //PAUSE pause song
					WriteOLEDString((uint8_t*)play_name[STATE], 0, 10);
					Tune_PauseSong();//might not work due to possible misalignment of pointer

				break;
			default:
				break;
		}
		break;


/****************************************************************************************************************
 *													tune2
 ****************************************************************************************************************/
	case TUNE2:
	//here joystick to manipulate tempo
																								//LED array manipulation
			if((((LPC_GPIOINT->IO2IntStatR) >> 3)& 0x1) == ENABLE){//if Joystick UP flag is set
				//increment
				Tune_SetTempo(index_change(1));
			}
			else if((((LPC_GPIOINT->IO0IntStatR) >> 15)& 0x1) == ENABLE){//if Joystick DOWN flag
				//decrement
				Tune_SetTempo(index_change(0));
			}
			//clear flags
			//GPIO_ClearInt(0, 1<<15); GPIO_ClearInt(2, 1<<3);

	//rotary stick for pitch
			if((((LPC_GPIOINT->IO0IntStatR) >> 24)& 0x1) == ENABLE){//if rotary stick CLOCKWISE flag is set
				//increment
				Tune_SetPitch(index_change2(1));
			}
			else if((((LPC_GPIOINT->IO0IntStatR) >> 25)& 0x1) == ENABLE){//if if rotary stick ANTICLOCKWISE flag
				//decrement
				Tune_SetPitch(index_change2(0));
			}
			//clear flags
			//GPIO_ClearInt(0, (1<<25 | 1<<24));




			switch(STATE){//nested switch and case
				case STOP:  //STOP stop song
						WriteOLEDString((uint8_t*)play_name[STATE], 0, 10);
						Tune_StopSong();//pointer is set to NULL
					break;
				case PLAY:  //PLAY play song
						WriteOLEDString((uint8_t*)play_name[STATE], 0, 10);
						//if tune is playing do not enter again
						Tune_PlaySong(Tune_SampleSongs[1], tPin);
						//STATE = STOP;//after tune has finished playing set tune back to stop mode
						//reprint to OLED
						//WriteOLEDString((uint8_t*)play_name[STATE], 0, 10);
					break;
				case PAUSE: //PAUSE pause song
						WriteOLEDString((uint8_t*)play_name[STATE], 0, 10);
						Tune_PauseSong();
					break;
				default:
					break;

				}
		break;


/****************************************************************************************************************
 *													drive
 ****************************************************************************************************************/
	case DRIVE:

		//rotary switch and joystick; gear and direction

		//gear change for drive using rotary stick

		//here mode switch for drive using joystick
		switch(DSTATE){//if disturbed during switch will just pick the first true case
		case STOPPED:
			if((((LPC_GPIOINT->IO2IntStatR) >> 3)& 0x1) == ENABLE){DSTATE=FORWARD;}//up forwards
			else if((((LPC_GPIOINT->IO0IntStatR) >> 15)& 0x1) == ENABLE){DSTATE=BACKWARD;}//down backwards
			else if((((LPC_GPIOINT->IO0IntStatR) >> 16)& 0x1) == ENABLE){DSTATE=RIGHT;}//right right
			else if((((LPC_GPIOINT->IO2IntStatR) >> 4)& 0x1) == ENABLE){DSTATE=LEFT;}//left left
						break;
		case FORWARD:
			if((((LPC_GPIOINT->IO0IntStatR) >> 15)& 0x1) == ENABLE){DSTATE=STOPPED;}//down stop
			else if((((LPC_GPIOINT->IO0IntStatR) >> 16)& 0x1) == ENABLE){DSTATE=RIGHT;}//right right
			else if((((LPC_GPIOINT->IO2IntStatR) >> 4)& 0x1) == ENABLE){DSTATE=LEFT;}//left left
						break;
		case BACKWARD:
			if((((LPC_GPIOINT->IO2IntStatR) >> 3)& 0x1) == ENABLE){DSTATE=STOPPED;}//up stop
			else if((((LPC_GPIOINT->IO0IntStatR) >> 16)& 0x1) == ENABLE){DSTATE=RIGHT;}//right right
			else if((((LPC_GPIOINT->IO2IntStatR) >> 4)& 0x1) == ENABLE){DSTATE=LEFT;}//left left
						break;
		case RIGHT:
			if((((LPC_GPIOINT->IO2IntStatR) >> 4)& 0x1) == ENABLE){DSTATE=STOPPED;}//left stop
			else if((((LPC_GPIOINT->IO2IntStatR) >> 3)& 0x1) == ENABLE){DSTATE=FORWARD;}//up forwards
			else if((((LPC_GPIOINT->IO0IntStatR) >> 15)& 0x1) == ENABLE){DSTATE=BACKWARD;}//down backwards
						break;
		case LEFT:
			if((((LPC_GPIOINT->IO0IntStatR) >> 16)& 0x1) == ENABLE){DSTATE=STOPPED;}//right stop
			else if((((LPC_GPIOINT->IO2IntStatR) >> 3)& 0x1) == ENABLE){DSTATE=FORWARD;}//up forwards
			else if((((LPC_GPIOINT->IO0IntStatR) >> 15)& 0x1) == ENABLE){DSTATE=BACKWARD;}//down backwards
						break;
		default:
			break;
		}//GPIO_ClearInt(0, (1<<16|1<<15) ); // //clear all flags for the joystick
		 //GPIO_ClearInt(2, (1<<4|1<<3) ); //left up

		 //next cases

		switch(DSTATE){//before functions change OLED		 DRIVE is "forward/reverse/right t/left  t/stopped"
		case STOPPED:

			wheelDistance +=  DFR_GetRightWheelCount();
			wheelDistance +=  DFR_GetLeftWheelCount();
			DFR_ClearWheelCounts();//used for optical encoder value can create an accumulated value and add on every time in stop mode
			floatToString(wheelDistance,string2,3);
			WriteOLEDString(string2, 4, 10);
			DFR_DriveStop();//stop
			WriteOLEDString((uint8_t*)drive_name[DSTATE], 2, 8);
						break;
		case FORWARD://original says Wheel destinations won't actually work. probably because was in a continual while loop
			DFR_SetRightWheelDestination(10);
			DFR_SetLeftWheelDestination(10);
			WriteOLEDString((uint8_t*)drive_name[DSTATE], 2, 8);
			DFR_DriveForward(100);
			//DSTATE=STOPPED;
					//forwards drive
						break;
		case BACKWARD:
			DFR_SetRightWheelDestination(10);
			DFR_SetLeftWheelDestination(10);
			WriteOLEDString((uint8_t*)drive_name[DSTATE], 2, 8);
			DFR_DriveBackward(100);
			//DSTATE=STOPPED;
					//backwards drive
						break;
		case RIGHT:
			DFR_SetRightWheelDestination(10);//will need to change destination value for both wheels (must be the same) if we're to have right angles
			DFR_SetLeftWheelDestination(10);	//done through trial and error
			WriteOLEDString((uint8_t*)drive_name[DSTATE], 2, 8);
			DFR_DriveRight(100);
			//DSTATE=STOPPED;
					//turn right
						break;
		case LEFT:
			DFR_SetRightWheelDestination(10);
			DFR_SetLeftWheelDestination(10);
			WriteOLEDString((uint8_t*)drive_name[DSTATE], 2, 8);
			DFR_DriveLeft(100);
			//DSTATE=STOPPED;
					//turn left
						break;
		default:
			break;	//make the default stop
				}


		break; //break for DRIVE state

	default:
		break;// do nothing maybe should assign a default mode


	}



//Last enabling of all required external interrupts
				//	 B	  A	  	Cntr right down	 SW3
GPIO_ClearInt( 0, (1<<25|1<<24|1<<17|1<<16|1<<15|1<<4) );
				// left   up	encoders
GPIO_ClearInt( 2, (1<<12|1<<11|1<<4|1<<3) );



endNum =  SYSTICK_GetCurrentValue();
runTimeCPU = endNum - beginNum;
intCount = intCount + ((endNum - beginNum)/100000000);
	
}

