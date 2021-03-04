# Compilation instructions for Visual Studio

# 2 solutions
With the latest version of SDRSHARP without the sources.
With the sources of an older version of SDRSHARP on github.

Installation of SDRSHARP
Installing the SDRSharp plugin.Rtl_433.dll.
Load rtl_433.dll project into visual studio
In debug version, compile to the SDRSHARP folder.
Launch SDRSHARP, breakpoints must work.


# to return to the original version

to compile the original version:
check rtl_sdr paths. h,libusb-1.0. lib libusb. h rtlsdr.lib
change configuration type dll->exe
delete the rtl_433.def module definition file
comment on the following 3 define

//add code dll_rtl_433
//#define DLL_RTL_433
//no open sdr
//#define NO_OPEN_SDR
//#define fprintf my_fprintf
