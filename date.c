// Ported from x264 sources.

#include <windows.h>
#include <sys/timeb.h>
#include "definitions.h"

int64_t avs2yuv_mdate( void )
{
    struct timeb tb;
    ftime( &tb );
    return ((int64_t)tb.time * 1000 + (int64_t)tb.millitm) * 1000;
}
