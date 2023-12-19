/* Copyright (C) 2022 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

/* Common functions for accessing All GENCODE (hgc-based) tracks */
#include "common.h"
#include "gencodeTracksCommon.h"
#include "trackDb.h"

char *gencodeBaseAcc(char *acc, char *accBuf, int accBufSize)
/* get the accession with version number dropped. */
{
safecpy(accBuf, accBufSize, acc);
char *dot = strchr(accBuf, '.');
if (dot != NULL)
    *dot = '\0';
return accBuf;
}

char* gencodeGetVersion(struct trackDb *tdb)
/* get the GENCODE version for a track. */
{
return trackDbRequiredSetting(tdb, "wgEncodeGencodeVersion");
}

char *gencodeGetTableName(struct trackDb *tdb, char *tableBase)
/* Return the table name with the version attached.  This just leaks the memory
 * and lets exit() clean up.  It is tiny */
{
char table[64];
safef(table, sizeof(table), "%sV%s", tableBase, gencodeGetVersion(tdb));
return cloneString(table);
}

