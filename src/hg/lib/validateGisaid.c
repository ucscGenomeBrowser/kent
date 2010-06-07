/* This is a dummy function for non-GISAID server */
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
