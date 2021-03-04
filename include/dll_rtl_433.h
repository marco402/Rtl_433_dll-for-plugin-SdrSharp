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

//add code dll_rtl_433 
#define DLL_RTL_433
//no open sdr
#define NO_OPEN_SDR

#define export __declspec(dllexport)

#define fprintf my_fprintf

int my_fprintf(_Inout_ FILE *const _Stream,
        _In_z_ _Printf_format_string_ char const *const _Format, ...);

#endif /* INCLUDE_DLL_RTL_433_H_ */
