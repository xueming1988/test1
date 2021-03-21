/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Replacements for frequently missing libm functions
 */

#ifndef AVUTIL_LIBM_H
#define AVUTIL_LIBM_H

#include <math.h>
#include "config.h"
#include "attributes.h"




#if !HAVE_LRINTF || (ARCH_MIPS)
#if __GNUC__ < 4
static av_always_inline av_const long int lrintf(float x)
{
    return (int)(rint(x));
}
#endif
#endif /* HAVE_LRINTF */

#if !HAVE_ROUND || (ARCH_MIPS)
#if __GNUC__ < 4
static av_always_inline av_const double round(double x)
{
    return (x > 0) ? floor(x + 0.5) : ceil(x - 0.5);
}
#endif
#endif /* HAVE_ROUND */

#if !HAVE_ROUNDF || (ARCH_MIPS)
#if __GNUC__ < 4
static av_always_inline av_const float roundf(float x)
{
    return (x > 0) ? floor(x + 0.5) : ceil(x - 0.5);
}
#endif
#endif /* HAVE_ROUNDF */

#if !HAVE_TRUNC || (ARCH_MIPS)
#if __GNUC__ < 4
static av_always_inline av_const double trunc(double x)
{
    return (x > 0) ? floor(x) : ceil(x);
}
#endif
#endif /* HAVE_TRUNC */

#if !HAVE_TRUNCF || (ARCH_MIPS)
#if __GNUC__ < 4
static av_always_inline av_const float truncf(float x)
{
    return (x > 0) ? floor(x) : ceil(x);
}
#endif
#endif /* HAVE_TRUNCF */

#endif /* AVUTIL_LIBM_H */
