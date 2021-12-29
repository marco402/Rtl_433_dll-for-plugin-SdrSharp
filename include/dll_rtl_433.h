// **************************************************************************************
//                                dll_rtl_433.h 
//						  include for project Rtl_433.dll
//						call by Plugin Rtl_433 for SdrSharp
// **************************************************************************************
// Creative Commons Attrib Share-Alike License
// You are free to use/extend this library but please abide with the CC-BY-SA license:
// Attribution-NonCommercial-ShareAlike 4.0 International License
// http://creativecommons.org/licenses/by-nc-sa/4.0/
//
// Written by Marc Prieur (marco40_github@sfr.fr)
//
//History : V1.00 2021-04-01 - First release
//
//
// All text above must be included in any redistribution.
//
// **********************************************************************************

#ifndef INCLUDE_DLL_RTL_433_H_
#define INCLUDE_DLL_RTL_433_H_
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

/******************************************mise a jour rtl_433*************************************************
1-found all  DLL_RTL_433 in project
	src\data.c
	src\r_api.c
	src\rtl_433.c
	src\sdr.c
2-found all NO_OPEN_SDR in project
	src\rtl_433.c
	src\sdr.c
3-save all files found
4-paste all files new version rtl_433
5-load new files in project mainly new devices
6-compare and modifie files found, normaly it's more easy modifie old files.
**************************************************************************************************************/

//add code dll_rtl_433 
#define DLL_RTL_433
//no open sdr
#define NO_OPEN_SDR

#define export __declspec(dllexport)

#define fprintf my_fprintf

int my_fprintf(_Inout_ FILE *const _Stream,
        _In_z_ _Printf_format_string_ char const *const _Format, ...);

typedef void(__stdcall *prt_call_back_init)(char *);
void setPtrInit(prt_call_back_init ptr_init, intptr_t ptr_cfg);

#endif /* INCLUDE_DLL_RTL_433_H_ */
