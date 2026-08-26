/* perfTimer - collect labeled wall-clock timings for a request and emit them as JSON.
 *
 * A perfTimer is a small accumulator built on clock1000().  It works like uglyTime()
 * (each step records the milliseconds elapsed since the previous step) but stores the
 * results in a list instead of printing them, so a CGI can hand the numbers to its
 * JavaScript front end inside the normal JSON payload and let the browser show a timing
 * dialog.  It is fully generic - no cart or CGI dependency - so any tool can adopt it.
 *
 * Typical use (guarded by a "measureTiming" cart/CGI var):
 *   struct perfTimer *pt = measureTiming ? perfTimerNew() : NULL;
 *   ... load cart ...
 *   perfTimerStep(pt, "startup + cart");
 *   ... run query ...
 *   perfTimerStep(pt, "load from MySQL");
 *   ...
 *   perfTimerJson(pt, jw, "timing");   // adds "timing": [ {label, ms}, ..., total ]
 *   perfTimerFree(&pt);
 *
 * Every function tolerates a NULL perfTimer, so callers can leave the perfTimerStep()
 * calls in place unconditionally and only pay for timing when it is turned on. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef PERFTIMER_H
#define PERFTIMER_H

struct jsonWrite;

struct perfTimerStep
/* One recorded interval: a label and the milliseconds spent since the previous step. */
    {
    struct perfTimerStep *next;
    char *label;		/* What this interval measured. */
    long ms;			/* Milliseconds elapsed during it. */
    };

struct perfTimer
/* Accumulates labeled timings for a request. */
    {
    long startTime;		/* clock1000() when the timer was created. */
    long lastTime;		/* clock1000() at the previous step. */
    struct perfTimerStep *steps;	/* Steps in reverse order until perfTimerJson. */
    };

struct perfTimer *perfTimerNew(void);
/* Return a new perfTimer with the clock started now.  Create as early as possible in a
 * request so the first step captures startup/cart-load time. */

void perfTimerStep(struct perfTimer *pt, char *label);
/* Record the milliseconds elapsed since the previous step (or since perfTimerNew) under
 * label, and reset the mark to now.  No-op if pt is NULL. */

void perfTimerJson(struct perfTimer *pt, struct jsonWrite *jw, char *name);
/* Emit "name": [ {"label":..,"ms":..}, ..., {"label":"total","ms":..} ] into jw, where
 * total is the milliseconds since perfTimerNew.  No-op if pt is NULL. */

void perfTimerFree(struct perfTimer **pPt);
/* Free a perfTimer and its steps. */

#endif /* PERFTIMER_H */
