#ifndef __DEFINES_H_INCLUDED__
#define __DEFINES_H_INCLUDED__

#ifdef __GNUC__
#include <stdint.h>
#define BYTE uint8_t
#else
#ifdef _MSC_VER 
#define BYTE unsigned __int8
#else
#define BYTE unsigned char
#endif
#endif

#define TO_WORD(h,l) ((l) + (h)*256) 

#endif
