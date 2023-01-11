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

/******************************************update rtl_433*************************************************

1-replaces folders src and include, keep file dll_rtl_433.h

process all DLL_RTL_433

nombre de devices 12/2021: 177/208  202/208
nombre de devices 01/2023: 192/223  217/223

**************************************************************************************************************/

//add code dll_rtl_433 
#define DLL_RTL_433
//no open sdr
//#define DLL_RTL_433

#define export __declspec(dllexport)

#define fprintf my_fprintf

int my_fprintf(_Inout_ FILE *const _Stream,
        _In_z_ _Printf_format_string_ char const *const _Format, ...);

typedef void(__stdcall *prt_call_back_init)(char *);
typedef void(__stdcall *prt_call_back_RecordOrder)(char *);
void setPtrInit(prt_call_back_init ptr_init, intptr_t ptr_cfg);

#endif /* INCLUDE_DLL_RTL_433_H_ */
