/* This is a dummy function for non-GISAID server */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"

#include "hCommon.h"

boolean validateGisaidUser()
{
if (hIsGisaidServer())
    {
    /* The real implementation has confidential logic, which could not be exposed */
    errAbort("The correct validateGisaidUser() implementation is not installed on this GISAID server.");
    return FALSE;
    }
else
    {
    return TRUE;
    }
}
