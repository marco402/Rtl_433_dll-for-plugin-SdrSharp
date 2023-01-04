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

***************************************12/2022************************************************


nombre de devices 12/2021: 177/208  202/208


1>c:\marc\tnt\sources\sdrsharp\rtl_433_dll\src\devices\abmt.c(93): error C2065: 'OOK_PULSE_PCM' : identificateur non déclaré
1>c:\marc\tnt\sources\sdrsharp\rtl_433_dll\src\devices\atech_ws308.c(132): error C2065: 'OOK_PULSE_RZ' : identificateur non déclaré


***************************************12/2021************************************************
sources .c
	rtl_433.c
				header
				includes
				6*#ifdef DLL_RTL_433
				1*#ifndef DLL_RTL_433
				1*#ifdef DLL_RTL_433
				3*#ifndef DLL_RTL_433 
				1*#else  DLL_RTL_433 
	sdr.c			add:
				header
				#includes "dll_rtl_433.h" //for fprintf
				#ifdef DLL_RTL_433  	1 fois
				#ifdef DLL_RTL_433	2 fois
 
add#include "dll_rtl_433.h" //for fprintf to output_file.c


	data.c			#include "dll_rtl_433.h" //for fprintf
	r_api.c			#include "dll_rtl_433.h" //for fprintf

  devices .c
	update for zombi with decoder_output_bitbufferf and verbose >1
	digitech_xc0324.c   add #include "dll_rtl_433.h" //for fprintf and #ifndef DLL_RTL_433 //window zombi if -vvv
	ikea_sparsnas.c     add #include "dll_rtl_433.h" //for fprintf and 3 fois #ifndef DLL_RTL_433 //window zombi if -vvv
	found decoder_output_bitbufferf if new
add new file output_file.c
	copy  2*#ifdef DLL_RTL_433 de data.c to output_file.c
		change *2  output->file to kv->file

		
  include .h
	add      	dll_rtl_433.h-------->copy
	rtl_433.h	add #define DEFAULT_DISABLED 0      //DLL_RTL_433
	sdr.h		no uodate for dll

for this version only(rtl_433-21.12)  rtl_433.c->rtl_433_call_main->_param_sample_size*2 

SdrSharp.Rtl_433_Plugin

verify if change structure and enum in NativeMethods.cs with project rtl_433 r_private.h,pulse_detect.h,sdr.c,baseband.h...

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
