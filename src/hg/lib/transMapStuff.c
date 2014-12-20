/* transMapStuff - common definitions and functions for supporting transMap
 * tracks in the browser CGIs */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "transMapStuff.h"
#include "trackDb.h"
#include "hdb.h"

char* transMapSkipGenomeDbPrefix(char *id)
/* Skip the source genome db prefix (e.g. hg19:) in a TransMap identifier.
 * Return the full id if no db prefix is found for compatibility with older
 * version of transmap. */
{
char *simpleId = strchr(id, ':');
if (simpleId == NULL)
    return id;
else
    return simpleId+1;
}

char *transMapIdToSeqId(char *id)
/* remove all unique suffixes (starting with last `-') from any TransMap 
 * id, leaving the database prefix in place.  WARNING: static return */
{
static char acc[128];
safecpy(acc, sizeof(acc), id);
char *dash = strrchr(acc, '-');
if (dash != NULL)
    *dash = '\0';
return acc;
}

char *transMapIdToAcc(char *id)
/* remove database prefix and all unique suffixes (starting with last `-')
 * from any TransMap id.  WARNING: static return */
{
static char acc[128];
safecpy(acc, sizeof(acc), transMapSkipGenomeDbPrefix(transMapIdToSeqId(id)));
return acc;
}
