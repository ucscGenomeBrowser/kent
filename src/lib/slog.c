/* slog - fixed point scaled logarithm stuff. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "slog.h"

static char const rcsid[] = "$Id: slog.c,v 1.4 2003/05/06 07:33:44 kate Exp $";

double fSlogScale = 8192.0;	/* Convert to fixed point by multiplying by this. */
double invSlogScale = 0.0001220703125; /* Conver back to floating point with this. */

int slog(double val)
/* Return scaled log. */
{
return (round(fSlogScale*log(val)));
}

int carefulSlog(double val)
/* Returns scaled log that makes sure there's no int overflow. */
{
if (val < 0.0000001)
    val = 0.0000001;
return slog(val);
}

double invSlog(int scaledLog)
/* Inverse of slog. */
{
return exp(scaledLog*invSlogScale);
}

