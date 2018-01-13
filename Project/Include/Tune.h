/***************************************************************************//**
 *
 * @file		Tune.h
 * @brief		Header file for a timer interrupt speaker driver
 * @author		Geoffrey Daniels, Dimitris Agrafiotis
 * @version		1.0
 * @date		06 August. 2012
 * @warning		Initialize GPIO before calling any functions in this file.
 *
 * Copyright(C) 2012, University of Bristol
 * All rights reserved.
 *
*******************************************************************************/

#ifndef TUNE_H
#define TUNE_H

#define TUNE_INCLUDE_SAMPLESONGS

/// @brief 		Sample songs, structure: {NDP}{NDP}... Note, Duration, Pause
#ifdef TUNE_INCLUDE_SAMPLESONGS //notes for the song
static char *Tune_SampleSongs[] = {//static value of char pointer retains value after function has ended
   "E2,E2,E4,E2,E2,E4,E2,G2,C2,D2,E8,F2,F2,F2,F2,F2,E2,E2,E2,E2,D2,D2,E2,D4,G4,E2,E2,E4,E2,E2,E4,E2,G2,C2,D2,E8,F2,F2,F2,F2,F2,E2,E2,E2,G2,G2,F2,D2,C8.",
   "D4,B4,B4,A4,A4,G4,E4,D4.D2,E4,E4,A4,F4,D8.D4,d4,d4,c4,c4,B4,G4,E4.E2,F4,F4,A4,A4,G8.",
};//static char *Tune_SampleSongs[] creates an array of character pointers, these have the value of the memory address of a the first character
  //above there are only two character pointers (array size is two)
  //Tune_SampleSongs[0] points to the first character of the first song
  //Tune_SampleSongs[1] points to the first character of second song
  //typedef enum {false, true} bool;

static const char Tune_SampleSongCount = 2; //possibly the number of songs
#endif

/// @brief 		Initialize the tune driver
/// @warning	Initialize GPIO before calling any functions in this file.
void Tune_Init(void);

/// @brief 		Check if the tune is playing
///	@returns	1 if the tune is playing otherwise 0
/// @warning	Initialize the tune driver before running this function
uint8_t Tune_IsPlaying(void);

/// @brief      Play a song
/// @param[in]  A null-terminated string of ascii notes, ensure it is static
/// @warning	Initialize the tune driver before running this function
void Tune_PlaySong(char* SongString, int pin);

/// @brief 		Check if the tune is paused
///	@returns	1 if the tune is paused otherwise 0
/// @warning	Initialize the tune driver before running this function
uint8_t Tune_IsPaused(void);

/// @brief      Pause playing
/// @warning	Initialize the tune driver before running this function
void Tune_PauseSong(void);

/// @brief      Stop playing
/// @warning	Initialize the tune driver before running this function
void Tune_StopSong(void);

/// @brief      Set song tempo
/// @param[in]  A positive number to speed up, negative to slow down
/// @warning	Initialize the tune driver before running this function
void Tune_SetTempo(int8_t Tempo);

/// @brief      Set song pitch
/// @param[in]  A positive number to go up, negative to go down
/// @warning	Initialize the tune driver before running this function
void Tune_SetPitch(int8_t Pitch);


uint32_t MiliToMicroSec(uint32_t x);

#endif // TUNE_H
