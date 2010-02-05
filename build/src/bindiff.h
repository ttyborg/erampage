/*
BinDiff
A set of functions for binary diff/patch

    BinDiff is free software, and you are welcome to redistribute it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version. 
    This software comes with ABSOLUTELY NO WARRANTY; see gpl.html for
    details. If you haven't received a copy of the GNU General Public
    License along with this program, see http://www.gnu.org/licenses/

Refer to  bindiff.html for help
Copyright 2010 Dmitry Grigoryev
*/

#include "streamio.h"

#ifndef __BINDIFF_H
#define __BINDIFF_H
#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _SIZE_T_DEFINED
typedef unsigned int size_t;
#define _SIZE_T_DEFINED
#endif

#ifndef NULL
#define NULL 0
#endif


typedef struct {
	stream_p    in1;
	stream_p    in2;
	void       *userdata;
	int         isPatch;
} bindiff_t, *bindiff_p;

#define BIN_DIFF_INIT(original, modified) {original, modified, NULL, 0}
#define BIN_PATCH_INIT(input, patch) {input, patch, NULL, 1}
#define BIN_DIFFSTREAM_INIT(in) {in, &BinDiff, NULL, &BinDiffClose}

int  BinDiff(void *in, void *buffer, size_t size);
void BinDiffClose(void *in);

#ifdef __cplusplus
}
#endif
#endif // __BINDIFF_H
