/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
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

