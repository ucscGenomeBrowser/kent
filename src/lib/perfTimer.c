/* perfTimer - collect labeled wall-clock timings for a request and emit them as JSON.
 * See perfTimer.h for how to use it. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "portable.h"
#include "jsonWrite.h"
#include "perfTimer.h"

struct perfTimer *perfTimerNew(void)
/* Return a new perfTimer with the clock started now. */
{
struct perfTimer *pt;
AllocVar(pt);
pt->startTime = pt->lastTime = clock1000();
return pt;
}

void perfTimerStep(struct perfTimer *pt, char *label)
/* Record the milliseconds elapsed since the previous step under label, and reset the
 * mark.  No-op if pt is NULL. */
{
if (pt == NULL)
    return;
long now = clock1000();
struct perfTimerStep *step;
AllocVar(step);
step->label = cloneString(label);
step->ms = now - pt->lastTime;
slAddHead(&pt->steps, step);
pt->lastTime = now;
}

void perfTimerJson(struct perfTimer *pt, struct jsonWrite *jw, char *name)
/* Emit "name": [ {"label":..,"ms":..}, ..., {"label":"total","ms":..} ] into jw.
 * No-op if pt is NULL. */
{
if (pt == NULL)
    return;
jsonWriteListStart(jw, name);
struct perfTimerStep *step;
slReverse(&pt->steps);   /* Restore chronological order. */
for (step = pt->steps; step != NULL; step = step->next)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "label", step->label);
    jsonWriteNumber(jw, "ms", step->ms);
    jsonWriteObjectEnd(jw);
    }
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "label", "total");
jsonWriteNumber(jw, "ms", clock1000() - pt->startTime);
jsonWriteObjectEnd(jw);
jsonWriteListEnd(jw);
}

void perfTimerFree(struct perfTimer **pPt)
/* Free a perfTimer and its steps. */
{
struct perfTimer *pt = *pPt;
if (pt == NULL)
    return;
struct perfTimerStep *step, *next;
for (step = pt->steps; step != NULL; step = next)
    {
    next = step->next;
    freeMem(step->label);
    freeMem(step);
    }
freez(pPt);
}
