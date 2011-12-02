/* snofSig - signature (first 16 bytes) of a snof format file. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "snofmake.h"


static int ixSig[4] = {0x693F8ED1, 0x7EDA1C32, 0x4BA58983, 0x277CB89C,};

void snofSignature(char **rSig, int *rSigSize)
/* Return signature. */
{
*rSig = (char *)ixSig;
*rSigSize = sizeof(ixSig);
}

boolean isSnofSig(void *sig)
/* Return true if sig is right. */
{
return memcmp(sig, ixSig, sizeof(ixSig)) == 0;
}

