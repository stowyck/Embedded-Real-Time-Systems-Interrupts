/***************************************************************************//**
 *
 * @file		Tune.c
 * @brief		Source file for the speaker driver
 * @author		Geoffrey Daniels, Dimitris Agrafiotis
 * @version		1.0
 * @date		19 July. 2012
 * @warning		Initialize GPIO before calling any functions in this file.
 *
 * Copyright(C) 2012, University of Bristol
 * All rights reserved
 *
*******************************************************************************/

// Includes
#include "LPC17xx_GPIO.h"

#include "Tune.h"

//------------------------------------------------------------------------------

// Defines and typedefs
#define SPEAKER_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define SPEAKER_PIN_LOW()  GPIO_ClearValue(0, 1<<26);


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


// To calculate: ( 1 Second / Note Frequency (Hz) ) * 1000000 Seconds / Microsecond
// Note A4, B4, a5 and b5 are out of order to enable simpler array lookups
static uint32_t Notes[] = {//some notes are missing i think??
    2272, // A4 - 440 Hz
    2024, // B4 - 494 Hz
    3816, // C4 - 262 Hz  <- Middle C
    3401, // D4 - 294 Hz
    3030, // E4 - 330 Hz
    2865, // F4 - 349 Hz
    2551, // G4 - 392 Hz
    1136, // a5 - 880 Hz
    1012, // b5 - 988 Hz
    1912, // c5 - 523 Hz
    1703, // d5 - 587 Hz
    1517, // e5 - 659 Hz
    1432, // f5 - 698 Hz
    1275, // g5 - 784 Hz
};


static double TempoScaling[] = {2.5,2,1.5,1,0.65,0.5,0.35,0.25};//scaling for Tempo
static volatile uint8_t k = 3;
static double PitchScaling[] = {0.2,0.4,0.6,0.8,1,2,3,4};//scaling for Pitch change range to 0.1 to 10
static volatile uint8_t m = 4;



//------------------------------------------------------------------------------

// External global variables
//...
typedef enum  {PLAY, PAUSE, STOP} song_type;
extern volatile song_type STATE;

extern volatile uint32_t beginNum; // initial value of interrupt timer
extern	volatile uint32_t endNum; 	// end value of interrupt timer
extern	volatile uint32_t counterIncrement; //1ms interrupt increments
extern	volatile uint32_t runTimeCPU;

extern	volatile uint32_t intCount; // saves duration of interrupt handling

	//uint32_t runTimeCPU; // available CPU time
extern	volatile float oneSecond;
extern	char string[2];




//------------------------------------------------------------------------------

// Local variables
static char* SongStringPointer = NULL;
static volatile uint32_t CurrentNote = 0;
static volatile uint32_t CurrentDuration = 0;
static volatile uint32_t CurrentPause = 0;

//local Variable
volatile static uint32_t Time = 0;//not sure if i should make static or not
volatile static uint32_t repeat = 1;
volatile static uint32_t initialised = 0;

//------------------------------------------------------------------------------

// Local Functions
static uint32_t GetNote(uint8_t Character)
{
    if ((Character >= 'A') && (Character <= 'G'))
        return Notes[Character - 'A'];

    if ((Character >= 'a') && (Character <= 'g'))
        return Notes[Character - 'a' + 7];

    return 0;
}

static uint32_t GetDuration(uint8_t Character)
{
    if (Character < '0' || Character > '9')
        return 400;

    // Number of ms
    return (Character - '0') * (200);
}

static uint32_t GetPause(uint8_t Character)
{
    switch (Character)
    {
    case '+':
        return 0;
    case ',':
        return 5;
    case '.':
        return 20;
    case '_':
        return 30;
    default:
        return 5;
    }
}




void PlayNote(){

	uint32_t Time = 0;
	while (Time < (CurrentDuration * 1000)){Time += CurrentNote;}
	LPC_GPIO0->FIOCLR = (1<<26);

}



static void PlaySong(void)
{	//so here local string pointer variable SongStringPointer gets value SongString which was the argument sent of the address value of the first character in the string;
	//LPC_TIM1->MCR  |= (1<<SBIT_MR0I); //enable interrupts again
	if(*SongStringPointer != '\0'){//when at end of string do not enter

		repeat = 1;
		initialised = 1;

		//if we reach the null character set local pointer back to NULL return to Tune_PlaySong, and that's it main program continues
		if (*SongStringPointer == 0) { Tune_StopSong(); return; } //function is called. If we are at the end of the tone string we return
		CurrentNote = PitchScaling[m]*GetNote(*SongStringPointer++); //otherwise GetNote takes the current character, then SongStringPointer points to the next character???
						//GetNote takes said character and returns the frequency/time period associated with that not from the Notes[] array, this is assigned to CurrentNote
						//when the pointer points to a non alphabetical character e.g. a number in the string or a comma , or whitespace CurrentNote gets the value zero

		//current duration changes tempo
		if (*SongStringPointer == 0) { Tune_StopSong(); return; }//using the static type so variables retains value outside function
		CurrentDuration = TempoScaling[k]*GetDuration(*SongStringPointer++);//reads the number next to the Note in the string
																				//and then returns a value of (Note number * 200)milliseconds, returns default value of 400ms if note number is not between 0 and 9
		if (*SongStringPointer == 0) { Tune_StopSong(); return; }
		CurrentPause = GetPause(*SongStringPointer++); //reads the pause character in string and assigns a corresponding value to CurrentPause
													// '+' 0	',' 5		'.' 20		'_' 30 		default 5;
		// Play note
		//PlayNote(); //so this should play a combination of all three


		LPC_TIM3->TCR |= 1 << 1; // Manually Reset Timer0 ( forced )
		LPC_TIM2->TCR |= 1 << 1; // Manually Reset Timer1 ( forced )
		LPC_TIM0->TCR |= 1 << 1; // Manually Reset Timer2 ( forced )
		LPC_TIM1->TCR |= 1 << 1; // Manually Reset Timer3 ( forced )
		
		
										//after reviewing code don't know why/forgotten why first argument is a 2 and not a 3 probably why i added a delay		//processing delay and linear realignment
		//LPC_TIM3->MR0 = converUSecToVal(2, MiliToMicroSec(CurrentDuration + CurrentPause) - 110000); //- 110000 have to subtract at least 100000us which is 0.1 seconds don't know why shift delay for programming maybe
		
				//function used to get prescalar and get microsecond value to use remove static type for function in file LPC17xx_Timer.c
		LPC_TIM3->MR0 = converUSecToVal(3, MiliToMicroSec(CurrentDuration + CurrentPause) - 110000); //- 110000 possibly not needed after change could not check as only noticed after ERTS unit was over, however above comment worked in the demonstration
		LPC_TIM2->MR0 = converUSecToVal(2, MiliToMicroSec(CurrentDuration)+ 100);//processing delay and linear realignment //added delay of 100mcs somehow needed don't know why got lucky for once??
		LPC_TIM0->MR0 = converUSecToVal(0, (CurrentNote/2));//speaker pin 
		LPC_TIM1->MR0 = converUSecToVal(1,400); //load a small time to start timer;


		LPC_TIM1->TCR &= ~(1 << 1); // stop resetting the timer.
		LPC_TIM1->TCR  = (1 <<SBIT_CNTEN); //start timer1


	}//we break while loop when we reach end of string and hence go back to main function
	else{
		STATE = STOP;//change state
		WriteOLEDString((uint8_t*)" STOP", 0, 10);//and print it in to screen
	}

	//overall plays note for half the period associated with letter of the note then low for same period, creating square wave of sound, repeats this for (note number period) seconds


}

//------------------------------------------------------------------------------

// Public Functions
void Tune_Init(void)
{
    GPIO_SetDir(2, 1<<0, 1);
    GPIO_SetDir(2, 1<<1, 1);

    GPIO_SetDir(0, 1<<27, 1);
    GPIO_SetDir(0, 1<<28, 1);
    GPIO_SetDir(2, 1<<13, 1);
    GPIO_SetDir(0, 1<<26, 1);

    GPIO_ClearValue(0, 1<<27); //LM4811-clk
    GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
    GPIO_ClearValue(2, 1<<13); //LM4811-shutdn
}

uint8_t Tune_IsPlaying(void)
{
	//...
	return 0;
}

void Tune_PlaySong(char* SongString, int pin)
{
	if (!SongString) return;//if the SongString has value zero, i.e. the argument that was sent which is an address somehow had  no value then return aka do nothing back to main.c

	endNum =  SYSTICK_GetCurrentValue();
	runTimeCPU = endNum - beginNum;
	intCount = intCount + ((endNum - beginNum)/100000000);
	runTimeCPU = 100*(intCount/oneSecond);
	floatToString(runTimeCPU,string,3);
	WriteOLEDString(string, 3, 9);
	oneSecond = 0;
	intCount =0;
	beginNum =  SYSTICK_GetCurrentValue();


	if(!pin){//if we are transitioning from pause to play do not assign SongStringPointer to SongString; set transition type back to zero
		SongStringPointer = SongString; //else pointer (local variable SongStringPointer) is assigned to beginning of song
	}
	LPC_TIM0->MCR  = (1<<SBIT_MR0I) | (1<<SBIT_MR0R) | (1<<SBIT_MR0S);  //enable interrupts on match register0 value, reset the timer counter TC on match register0, stop timer on match register0
	LPC_TIM1->MCR  = (1<<SBIT_MR0I) | (1<<SBIT_MR0R) | (1<<SBIT_MR0S);	//same thing //However may need to change so that we don't force it to call itself see online example
	LPC_TIM3->MCR  = (1<<SBIT_MR0I) | (1<<SBIT_MR0R) | (1<<SBIT_MR0S);
	LPC_TIM2->MCR  = (1<<SBIT_MR0I) | (1<<SBIT_MR0R) | (1<<SBIT_MR0S);
	PlaySong(); //here we play the song ***important function

	//should there be a return here ?????
}

uint8_t Tune_IsPaused(void)
{
	//...
	return 0;
}

void Tune_PauseSong(void)
{

	endNum =  SYSTICK_GetCurrentValue();
	runTimeCPU = endNum - beginNum;
	intCount = intCount + ((endNum - beginNum)/100000000);
	runTimeCPU = 100*(intCount/oneSecond);
	floatToString(runTimeCPU,string,3);
	WriteOLEDString(string, 3, 9);
	oneSecond = 0;
	intCount =0;

	beginNum =  SYSTICK_GetCurrentValue();

	//above used for cpu usage

	LPC_TIM0->MCR  &= ~(1<<SBIT_MR0S); //disable all interrupts
	LPC_TIM1->MCR  &= ~(1<<SBIT_MR0S);
	LPC_TIM3->MCR  &= ~(1<<SBIT_MR0S);
	LPC_TIM2->MCR  &= ~(1<<SBIT_MR0S);

	LPC_TIM0->MCR  &= ~(1<<SBIT_MR0I); //disable all matches
	LPC_TIM1->MCR  &= ~(1<<SBIT_MR0I);
	LPC_TIM3->MCR  &= ~(1<<SBIT_MR0I);
	LPC_TIM2->MCR  &= ~(1<<SBIT_MR0I);

	LPC_TIM0->TCR &= ~(1 << 0);			//stop the counter
	LPC_TIM1->TCR &= ~(1 << 0);
	LPC_TIM3->TCR &= ~(1 << 0);
	LPC_TIM2->TCR &= ~(1 << 0);


	LPC_TIM0->TCR |= 1 << 1;			//set timer back to zero
	LPC_TIM1->TCR |= 1 << 1;
	LPC_TIM3->TCR |= 1 << 1;
	LPC_TIM2->TCR |= 1 << 1;

	SPEAKER_PIN_LOW();

	return;

}



void Tune_StopSong(void)
{
	endNum =  SYSTICK_GetCurrentValue();
	runTimeCPU = endNum - beginNum;
	intCount = intCount + ((endNum - beginNum)/100000000);
	runTimeCPU = 100*(intCount/oneSecond);
	floatToString(runTimeCPU,string,3);
	WriteOLEDString(string, 3, 9);
	oneSecond = 0;
	intCount =0;
	beginNum = SYSTICK_GetCurrentValue();

	//above used for cpu usage

	LPC_TIM0->MCR  &= ~(1<<SBIT_MR0S); //disable all interrupts
	LPC_TIM1->MCR  &= ~(1<<SBIT_MR0S);
	LPC_TIM3->MCR  &= ~(1<<SBIT_MR0S);
	LPC_TIM2->MCR  &= ~(1<<SBIT_MR0S);

	LPC_TIM0->MCR  &= ~(1<<SBIT_MR0I); //disable all matches
	LPC_TIM1->MCR  &= ~(1<<SBIT_MR0I);
	LPC_TIM3->MCR  &= ~(1<<SBIT_MR0I);
	LPC_TIM2->MCR  &= ~(1<<SBIT_MR0I);

	LPC_TIM0->TCR &= ~(1 << 0);			//stop the counter
	LPC_TIM1->TCR &= ~(1 << 0);
	LPC_TIM3->TCR &= ~(1 << 0);
	LPC_TIM2->TCR &= ~(1 << 0);


	LPC_TIM0->TCR |= 1 << 1;			//set timer back to zero
	LPC_TIM1->TCR |= 1 << 1;
	LPC_TIM3->TCR |= 1 << 1;
	LPC_TIM2->TCR |= 1 << 1;

	SPEAKER_PIN_LOW();


	SongStringPointer = NULL;


}



void Tune_SetTempo(int8_t Tempo) //need to write the code for set tempo and pitch for the song
{
	//...						//send values from main program and use a static variable

	k = Tempo;				//local variable Tempo was in original code as with Pitch below

}

void Tune_SetPitch(int8_t Pitch)	//we change musical pitch by changing the frequency of the note
{
	m = Pitch;//...						//send values from main program and use a static varable
}



uint32_t MiliToMicroSec(uint32_t x){return x*1000;}





void TIMER0_IRQHandler(void) //working fine
{

	if(repeat == 1){
		LPC_TIM0->IR |= 1 << 0;
		LPC_GPIO0->FIOPIN &= ~(1<<26);//LPC_GPIO0->FIOPIN ^= (1<<26);	//set speaker low
		LPC_TIM0->MR0 =  converUSecToVal(0,CurrentNote);
		LPC_TIM0->TCR  = (1 <<SBIT_CNTEN);
			}//else{LPC_TIM0->IR |= 1 << 0;}
}

void TIMER1_IRQHandler(void)
{


		if(repeat == 1){
			LPC_TIM1->IR |= 1 << 0;
			LPC_GPIO0->FIOPIN |= (1<<26); //LPC_GPIO0->FIOPIN ^= (1<<26); //speaker pin high
			LPC_TIM1->MR0 =  converUSecToVal(1,CurrentNote);
			LPC_TIM1->TCR  = (1 <<SBIT_CNTEN);
		}//else{LPC_TIM1->IR |= 1 << 0;}

		if(initialised == 1){
			initialised = 0;
			LPC_TIM1->TCR &= ~(1 << 1); // stop resetting the timer.
			LPC_TIM1->TCR  = (1 <<SBIT_CNTEN); //start timer1
			LPC_TIM3->TCR &= ~(1 << 1); // stop resetting the timer. //may place these in ISR1
			LPC_TIM3->TCR  = (1 <<SBIT_CNTEN);
			LPC_TIM2->TCR &= ~(1 << 1); // stop resetting the timer.
			LPC_TIM2->TCR  = (1 <<SBIT_CNTEN);
			LPC_TIM0->TCR &= ~(1 << 1); // stop resetting the timer.
			LPC_TIM0->TCR  = (1 <<SBIT_CNTEN);


				}

}



void TIMER2_IRQHandler(void)//stops the note
{
	repeat = 0;
	LPC_TIM2->IR |= 1 << 0; //clear flags

}

void TIMER3_IRQHandler(void)//repeats for next note and duration
{
	LPC_TIM3->IR |= 1 << 0; //clear flags
	PlaySong();
}
