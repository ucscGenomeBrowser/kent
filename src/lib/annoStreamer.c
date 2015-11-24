/* annoStreamer -- returns items sorted by genomic position to successive nextRow calls */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoStreamer.h"
#include "errAbort.h"

// ----------------------- annoStreamer base methods --------------------------

struct asObject *annoStreamerGetAutoSqlObject(struct annoStreamer *self)
/* Return parsed autoSql definition of this streamer's data type. */
{
return self->asObj;
}

void annoStreamerSetAutoSqlObject(struct annoStreamer *self, struct asObject *asObj)
/* Use new asObj and update internal state derived from asObj. */
{
self->asObj = asObj;
self->numCols = slCount(asObj->columnList);
}

char *annoStreamerGetName(struct annoStreamer *self)
/* Returns cloned name of streamer; free when done. */
{
return cloneString(self->name);
}

void annoStreamerSetName(struct annoStreamer *self, char *name)
/* Sets streamer name to clone of name. */
{
freez(&(self->name));
self->name = cloneString(name);
}

void annoStreamerSetRegion(struct annoStreamer *self, char *chrom, uint rStart, uint rEnd)
/* Set genomic region for query; if chrom is NULL, position is genome.
 * Many subclasses should make their own setRegion method that calls this and
 * configures their data connection to change to the new position. */
{
freez(&(self->chrom));
if (chrom == NULL)
    {
    self->positionIsGenome = TRUE;
    self->regionStart = self->regionEnd = 0;
    }
else
    {
    self->positionIsGenome = FALSE;
    self->chrom = cloneString(chrom);
    self->regionStart = rStart;
    if (rEnd == 0)
	rEnd = annoAssemblySeqSize(self->assembly, chrom);
    self->regionEnd = rEnd;
    }
}

static char *annoStreamerGetHeader(struct annoStreamer *self)
/* Default method: return NULL. */
{
return NULL;
}

void annoStreamerSetFilters(struct annoStreamer *self, struct annoFilter *newFilters)
/* Replace any existing filters with newFilters.  It is up to calling code to
 * free old filters and allocate newFilters. */
{
self->filters = newFilters;
}

void annoStreamerAddFilters(struct annoStreamer *self, struct annoFilter *newFilters)
/* Add newFilter(s).  It is up to calling code to allocate newFilters. */
{
self->filters = slCat(newFilters, self->filters);
}

void annoStreamerInit(struct annoStreamer *self, struct annoAssembly *assembly,
		      struct asObject *asObj, char *name)
/* Initialize a newly allocated annoStreamer with default annoStreamer methods.
 * If asObj is NULL, then the caller must call annoStreamerSetAutoSqlObject before
 * configuring filters.
 * In general, subclasses' constructors will call this first; override nextRow, close,
 * and probably setRegion; and then initialize their private data. */
{
self->assembly = assembly;
self->getAutoSqlObject = annoStreamerGetAutoSqlObject;
self->setRegion = annoStreamerSetRegion;
self->getHeader = annoStreamerGetHeader;
self->setFilters = annoStreamerSetFilters;
self->addFilters = annoStreamerAddFilters;
self->positionIsGenome = TRUE;
if (asObj != NULL)
    annoStreamerSetAutoSqlObject(self, asObj);
if (name == NULL)
    errAbort("annoStreamerInit: need non-NULL name");
self->name = cloneString(name);
}

void annoStreamerFree(struct annoStreamer **pSelf)
/* Free self. This should be called at the end of subclass close methods, after
 * subclass-specific connections are closed and resources are freed. */
{
if (pSelf == NULL)
    return;
struct annoStreamer *self = *pSelf;
freez(&(self->name));
freez(&(self->chrom));
freez(pSelf);
}

INLINE boolean findColumn(struct asColumn *columns, char *name, int *retIx, char **retName)
/* Scan columns for name.
 * If found, set retIx to column index, set retName to clone of name, and return TRUE.
 * If not found, set retIx to -1, set retName to NULL, and return FALSE; */
{
int ix = asColumnFindIx(columns, name);
if (retIx != NULL)
    *retIx = ix;
if (retName != NULL)
    {
    if (ix >= 0)
	*retName = cloneString(name);
    else
	*retName = NULL;
    }
return (ix >= 0);
}

boolean annoStreamerFindBed3Columns(struct annoStreamer *self,
			    int *retChromIx, int *retStartIx, int *retEndIx,
			    char **retChromField, char **retStartField, char **retEndField)
/* Scan autoSql for recognized column names corresponding to BED3 columns.
 * Set ret*Ix to list index of each column if found, or -1 if not found.
 * Set ret*Field to column name if found, or NULL if not found.
 * If all three are found, return TRUE; otherwise return FALSE. */
{
struct asColumn *columns = self->asObj->columnList;
if (findColumn(columns, "chrom", retChromIx, retChromField))
    {
    if (findColumn(columns, "chromStart", retStartIx, retStartField))
	return findColumn(columns, "chromEnd", retEndIx, retEndField);
    else return (findColumn(columns, "txStart", retStartIx, retStartField) &&
		 findColumn(columns, "txEnd", retEndIx, retEndField));
    }
else if (findColumn(columns, "tName", retChromIx, retChromField))
    return (findColumn(columns, "tStart", retStartIx, retStartField) &&
	    findColumn(columns, "tEnd", retEndIx, retEndField));
else if (findColumn(columns, "genoName", retChromIx, retChromField))
    return (findColumn(columns, "genoStart", retStartIx, retStartField) &&
	    findColumn(columns, "genoEnd", retEndIx, retEndField));
return FALSE;
}

struct annoStreamRows *annoStreamRowsNew(struct annoStreamer *streamerList)
/* Returns an array of aSR, one for each streamer in streamerList.
 * Typically array is reused by overwriting elements' rowList pointers.
 * Free array when done. */
{
int streamerCount = slCount(streamerList);
struct annoStreamRows *data = NULL;
AllocArray(data, streamerCount);
struct annoStreamer *streamer = streamerList;
int i = 0;
for (;  i < streamerCount;  i++, streamer = streamer->next)
    data[i].streamer = streamer;
return data;
}
