/* annoStreamer -- returns items sorted by genomic position to successive nextRow calls */

#include "annoStreamer.h"
#include "errabort.h"

// ----------------------- annoStreamer base methods --------------------------

struct asObject *annoStreamerGetAutoSqlObject(struct annoStreamer *self)
/* Return parsed autoSql definition of this streamer's data type. */
{
return self->asObj;
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
    self->regionEnd = rEnd;
    }
}

static char *annoStreamerGetHeader(struct annoStreamer *self)
/* Default method: return NULL. */
{
return NULL;
}

struct annoFilter *annoStreamerGetFilters(struct annoStreamer *self)
/* Return supported filters with current settings.  Callers can modify and free when done. */
{
return annoFilterCloneList(self->filters);
}

void annoStreamerSetFilters(struct annoStreamer *self, struct annoFilter *newFilters)
/* Free old filters and use clone of newFilters. */
{
annoFilterFreeList(&(self->filters));
self->filters = annoFilterCloneList(newFilters);
}

struct annoColumn *annoStreamerGetColumns(struct annoStreamer *self)
/* Return supported columns with current settings.  Callers can modify and free when done. */
{
return annoColumnCloneList(self->columns);
}

void annoStreamerSetColumns(struct annoStreamer *self, struct annoColumn *newColumns)
/* Free old columns and use clone of newColumns. */
{
annoColumnFreeList(&(self->columns));
self->columns = annoColumnCloneList(newColumns);
}

void annoStreamerSetQuery(struct annoStreamer *self, struct annoGratorQuery *query)
/* Set query (to be called only by annoGratorQuery which is created after streamers). */
{
self->query = query;
}

void annoStreamerInit(struct annoStreamer *self, struct asObject *asObj)
/* Initialize a newly allocated annoStreamer with default annoStreamer methods and
 * default filters and columns based on asObj.
 * In general, subclasses' constructors will call this first; override nextRow, close,
 * and probably setRegion and setQuery; and then initialize their private data. */
{
self->getAutoSqlObject = annoStreamerGetAutoSqlObject;
self->setRegion = annoStreamerSetRegion;
self->getHeader = annoStreamerGetHeader;
self->getFilters = annoStreamerGetFilters;
self->setFilters = annoStreamerSetFilters;
self->getColumns = annoStreamerGetColumns;
self->setColumns = annoStreamerSetColumns;
self->setQuery = annoStreamerSetQuery;
self->positionIsGenome = TRUE;
self->asObj = asObj;
self->filters = annoFiltersFromAsObject(asObj);
self->columns = annoColumnsFromAsObject(asObj);
self->numCols = slCount(asObj->columnList);
}

void annoStreamerFree(struct annoStreamer **pSelf)
/* Free self. This should be called at the end of subclass close methods, after
 * subclass-specific connections are closed and resources are freed. */
{
if (pSelf == NULL)
    return;
struct annoStreamer *self = *pSelf;
freez(&(self->chrom));
annoFilterFreeList(&(self->filters));
annoColumnFreeList(&(self->columns));
freez(pSelf);
}
