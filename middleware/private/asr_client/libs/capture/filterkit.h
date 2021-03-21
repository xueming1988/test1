/**********************************************************************

  resamplesubs.c

  Real-time library interface by Dominic Mazzoni

  Based on resample-1.7:
    http://www-ccrma.stanford.edu/~jos/resample/

  Dual-licensed as LGPL and BSD; see README.md and LICENSE* files.

**********************************************************************/

/* Definitions */
#include "resample_defs.h"

/*
 * FilterUp() - Applies a filter to a given sample when up-converting.
 * FilterUD() - Applies a filter to a given sample when up- or down-
 */

float lrsFilterUp(float Imp[], float ImpD[], UWORD Nwing, BOOL Interp, float *Xp, Float Ph,
                  int Inc);

float lrsFilterUD(float Imp[], float ImpD[], UWORD Nwing, BOOL Interp, float *Xp, Float Ph, int Inc,
                  Float dhb);

void lrsLpFilter(Float c[], int N, Float frq, Float Beta, int Num);
