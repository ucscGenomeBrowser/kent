/* annoGratorQuery -- framework for integrating genomic annotations from many sources */

#include "annoGratorQuery.h"
#include "errabort.h"
#include "obscure.h"

struct annoGratorQuery *annoGratorQueryNew(char *assemblyName, struct hash *chromSizes,
					   struct twoBitFile *tbf,
					   struct annoStreamer *primarySource,
					   struct annoGrator *integrators,
					   struct annoFormatter *formatters)
/* Create an annoGratorQuery from all of its components, and introduce components to each other.
 * Either chromSizes or tbf may be NULL.  integrators may be NULL.
 * All other inputs must be non-NULL. */
{
if (assemblyName == NULL)
    errAbort("annoGratorQueryNew: assemblyName can't be NULL");
if (chromSizes == NULL && tbf == NULL)
    errAbort("annoGratorQueryNew: chromSizes and tbf can't both be NULL");
if (primarySource == NULL)
    errAbort("annoGratorQueryNew: primarySource can't be NULL");
if (formatters == NULL)
    errAbort("annoGratorQueryNew: formatters can't be NULL");
struct annoGratorQuery *query = NULL;
AllocVar(query);
if (tbf != NULL)
    {
    if (chromSizes != NULL)
	{
	// Ensure that tbf and chromSizes are consistent.
	struct hashEl *hel;
	struct hashCookie cookie = hashFirst(chromSizes);
	while ((hel = hashNext(&cookie)) != NULL)
	    {
	    char *chrom = hel->name;
	    int size = ptToInt(hel->val);
	    int tbfSize = twoBitSeqSize(tbf, chrom);
	    if (tbfSize != size)
		errAbort("Inconsistent size for %s: %s has %d but chromSizes hash has %d",
			 chrom, tbf->fileName, tbfSize, size);
	    }
	}
    else
	{
	// Make our own chromSizes from tbf info.  We will leak this but I don't expect
	// many annoGratorQuery's in the same process.
	chromSizes = hashNew(0);
	struct slName *tbfSeqs = twoBitSeqNames(tbf->fileName), *seq;
	for (seq = tbfSeqs;  seq != NULL;  seq = seq->next)
	    hashAddInt(chromSizes, seq->name, twoBitSeqSize(tbf, seq->name));
	query->csAllocdHere = TRUE;
	}
    }
query->assemblyName = cloneString(assemblyName);
query->chromSizes = chromSizes;
query->tbf = tbf;
query->primarySource = primarySource;
query->integrators = integrators;
query->formatters = formatters;
// Set streamers' and formatters' query pointer.
primarySource->setQuery(primarySource, query);
struct annoStreamer *grator = (struct annoStreamer *)(query->integrators);
for (;  grator != NULL;  grator = grator->next)
    grator->setQuery(grator, query);
struct annoFormatter *formatter;
for (formatter = query->formatters;  formatter != NULL;  formatter = formatter->next)
    formatter->initialize(formatter, query);
return query;
}

void annoGratorQuerySetRegion(struct annoGratorQuery *query, char *chrom, uint rStart, uint rEnd)
/* Set genomic region for query; if chrom is NULL, position is whole genome. */
{
if (chrom != NULL)
    {
    uint chromSize = (uint)hashIntVal(query->chromSizes, chrom);
    if (rEnd < rStart)
	errAbort("annoGratorQuerySetRegion: rStart (%u) can't be greater than rEnd (%u)",
		 rStart, rEnd);
    if (rEnd > chromSize)
	errAbort("annoGratorQuerySetRegion: rEnd (%u) can't be greater than chrom %s size (%u)",
		 rEnd, chrom, chromSize);
    if (rEnd == 0)
	rEnd = chromSize;
    }
// Alert all streamers that they should now send data from a possibly different region:
query->primarySource->setRegion(query->primarySource, chrom, rStart, rEnd);
struct annoStreamer *grator = (struct annoStreamer *)(query->integrators);
for (;  grator != NULL;  grator = grator->next)
    grator->setRegion(grator, chrom, rStart, rEnd);
//#*** formatters should be told too, in case the info should go in the header, or if
//#*** they should clip output to search region....
}

void annoGratorQueryExecute(struct annoGratorQuery *query)
/* For each annoRow from query->primarySource, invoke integrators and pass their annoRows
 * to formatters. */
{
struct annoStreamer *primarySrc = query->primarySource;
struct annoFormatter *formatter = NULL;
struct annoRow *primaryRow = NULL;
while ((primaryRow = primarySrc->nextRow(primarySrc)) != NULL)
    {
    if (primaryRow->rightJoinFail)
	continue;
    struct slRef *gratorRowList = NULL;
    boolean rjFilterFailed = FALSE;
    struct annoStreamer *grator = (struct annoStreamer *)(query->integrators);
    for (;  grator != NULL;  grator = grator->next)
	{
	struct annoGrator *realGrator = (struct annoGrator *)grator;
	struct annoRow *gratorRows = realGrator->integrate(realGrator, primaryRow, &rjFilterFailed);
	slAddHead(&gratorRowList, slRefNew(gratorRows));
	if (rjFilterFailed)
	    break;
	}
    slReverse(&gratorRowList);
    for (formatter = query->formatters;  formatter != NULL;  formatter = formatter->next)
	if (!rjFilterFailed)
	    formatter->formatOne(formatter, primaryRow, gratorRowList);
    annoRowFree(&primaryRow, primarySrc);
    struct slRef *oneRowList = gratorRowList;
    grator = (struct annoStreamer *)(query->integrators);
    for (;  oneRowList != NULL;  oneRowList = oneRowList->next, grator = grator->next)
	annoRowFreeList((struct annoRow **)&(oneRowList->val), grator);
    slFreeList(&oneRowList);
    }
}

void annoGratorQueryFree(struct annoGratorQuery **pQuery)
/* Close and free all inputs and outputs; free self. */
{
if (pQuery == NULL)
    return;
struct annoGratorQuery *query = *pQuery;
freez(&(query->assemblyName));
if (query->csAllocdHere)
    hashFree(&(query->chromSizes));
query->primarySource->close(&(query->primarySource));
struct annoStreamer *grator = (struct annoStreamer *)(query->integrators), *nextGrator;
for (;  grator != NULL;  grator = nextGrator)
    {
    nextGrator = grator->next;
    grator->close(&grator);
    }
struct annoFormatter *formatter, *nextFormatter;
for (formatter = query->formatters;  formatter != NULL;  formatter = nextFormatter)
    {
    nextFormatter = formatter->next;
    formatter->close(&formatter);
    }
freez(pQuery);
}
