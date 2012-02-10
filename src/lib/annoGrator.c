/* annoGrator -- object framework for integrating genomic annotations from disparate sources */

#include "common.h"
#include "annoGrator.h"

void annoGratorQueryExecute(struct annoGratorQuery *query)
/* For each annoRow from query->primarySource, invoke integrators and pass their annoRows
 * to formatters. */
{
struct annoStreamer *primarySrc = query->primarySource;
struct annoFormatter *formatter = NULL;
struct annoRow *primaryRow = NULL;
while ((primaryRow = primarySrc->nextRow(primarySrc, NULL)) != NULL)
    {
    boolean filterFailed = FALSE;
    for (formatter = query->formatters;  formatter != NULL;  formatter = formatter->next)
	formatter->collect(formatter, primarySrc, primaryRow, filterFailed);
    struct annoGrator *grator;
    for (grator = query->integrators;  grator != NULL;  grator = grator->next)
	{
	struct annoRow *gratorRows = grator->integrate(grator, primarySrc, primaryRow,
						       &filterFailed);
	for (formatter = query->formatters;  formatter != NULL;  formatter = formatter->next)
	    formatter->collect(formatter, grator->source, gratorRows, filterFailed);
	if (filterFailed)
	    break;
	}
    if (!filterFailed)
	for (formatter = query->formatters;  formatter != NULL;  formatter = formatter->next)
	    formatter->formatOne(formatter);
    }
for (formatter = query->formatters;  formatter != NULL;  formatter = formatter->next)
    formatter->close(&formatter);
}

void annoGratorQueryFree(struct annoGratorQuery **pQuery)
/* Close and free all inputs and outputs; free self. */
{
if (pQuery == NULL)
    return;
struct annoStreamer *primarySrc = (*pQuery)->primarySource;
primarySrc->close(&primarySrc);
struct annoGrator *grator, *nextGrator;
for (grator = (*pQuery)->integrators;  grator != NULL;  grator = nextGrator)
    {
    nextGrator = grator->next;
    grator->close(&grator);
    }
struct annoFormatter *formatter, *nextFormatter;
for (formatter = (*pQuery)->formatters;  formatter != NULL;  formatter = formatter->next)
    {
    nextFormatter = formatter->next;
    formatter->close(&formatter);
    }
freez(pQuery);
}
