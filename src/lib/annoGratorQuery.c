/* annoGratorQuery -- framework for integrating genomic annotations from many sources */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoGratorQuery.h"
#include "dystring.h"
#include "errAbort.h"
#include "obscure.h"

struct annoGratorQuery
/* Representation of a complex query: multiple sources, each with its own filters,
 * output data and means of integration, aggregated and output by a formatter. */
    {
    struct annoAssembly *assembly;	// Genome assembly to which annotations belong
    struct annoStreamer *primarySource;	// Annotations to be integrated with other annos.
    struct annoGrator *integrators;	// Annotations & methods for integrating w/primary
    struct annoFormatter *formatters;	// Writers of output collected from primary & intg's
    };

struct annoGratorQuery *annoGratorQueryNew(struct annoAssembly *assembly,
					   struct annoStreamer *primarySource,
					   struct annoGrator *integrators,
					   struct annoFormatter *formatters)
/* Create an annoGratorQuery from all of its components, and introduce components to each other.
 * integrators may be NULL.  All other inputs must be non-NULL. */
{
if (assembly == NULL)
    errAbort("annoGratorQueryNew: assembly can't be NULL");
if (primarySource == NULL)
    errAbort("annoGratorQueryNew: primarySource can't be NULL");
if (formatters == NULL)
    errAbort("annoGratorQueryNew: formatters can't be NULL");
struct annoGratorQuery *query = NULL;
AllocVar(query);
query->assembly = assembly;
query->primarySource = primarySource;
query->integrators = integrators;
query->formatters = formatters;
struct annoFormatter *formatter;
for (formatter = query->formatters;  formatter != NULL;  formatter = formatter->next)
    formatter->initialize(formatter, primarySource, (struct annoStreamer *)integrators);
return query;
}

void annoGratorQuerySetRegion(struct annoGratorQuery *query, char *chrom, uint rStart, uint rEnd)
/* Set genomic region for query; if chrom is NULL, position is whole genome. */
{
if (chrom != NULL)
    {
    uint chromSize = annoAssemblySeqSize(query->assembly, chrom);
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
}

void annoGratorQueryExecute(struct annoGratorQuery *query)
/* For each annoRow from query->primarySource, invoke integrators and pass their annoRows
 * to formatters. */
{
struct annoStreamer *primarySrc = query->primarySource;
struct annoStreamRows *primaryData = annoStreamRowsNew(primarySrc);
struct annoStreamRows *gratorData = NULL;
int gratorCount = slCount(query->integrators);
if (gratorCount > 0)
    {
    struct annoStreamer *gratorStreamList = (struct annoStreamer *)query->integrators;
    gratorData = annoStreamRowsNew(gratorStreamList);
    }
char *regionChrom = primarySrc->chrom;
struct annoRow *primaryRow = NULL;
struct lm *lm = lmInit(0);
boolean gotPrimaryData = FALSE;
while ((primaryRow = primarySrc->nextRow(primarySrc, NULL, 0, lm)) != NULL)
    {
    if (regionChrom != NULL && strcmp(primaryRow->chrom, regionChrom) > 0)
	{
	// primarySrc's first row is on some chromosome past regionChrom, i.e. it has no
	// items on regionChrom.
	break;
	}
    if (primaryRow->rightJoinFail)
	continue;
    gotPrimaryData = TRUE;
    primaryData->rowList = primaryRow;
    boolean rjFilterFailed = FALSE;
    int i;
    for (i = 0;  i < gratorCount;  i++)
	{

	struct annoGrator *grator = (struct annoGrator *)gratorData[i].streamer;
	gratorData[i].rowList = grator->integrate(grator, primaryData, &rjFilterFailed, lm);
	if (rjFilterFailed)
	    break;
	}
    struct annoFormatter *formatter = NULL;
    for (formatter = query->formatters;  formatter != NULL;  formatter = formatter->next)
	if (!rjFilterFailed)
	    formatter->formatOne(formatter, primaryData, gratorData, gratorCount);
    lmCleanup(&lm);
    lm = lmInit(0);
    }
if (!gotPrimaryData)
    {
    struct dyString *dy = dyStringCreate("No data from %s", primarySrc->name);
    if (regionChrom != NULL)
	dyStringPrintf(dy, " in range %s:%d-%d; try changing 'region to annotate' to 'genome'",
		       regionChrom, primarySrc->regionStart, primarySrc->regionEnd);
    struct annoFormatter *formatter = NULL;
    for (formatter = query->formatters;  formatter != NULL;  formatter = formatter->next)
	formatter->comment(formatter, dy->string);
    dyStringFree(&dy);
    }
freez(&primaryData);
freez(&gratorData);
lmCleanup(&lm);
}

void annoGratorQueryFree(struct annoGratorQuery **pQuery)
/* Close and free all inputs and outputs; free self. */
{
if (pQuery == NULL)
    return;
struct annoGratorQuery *query = *pQuery;
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
