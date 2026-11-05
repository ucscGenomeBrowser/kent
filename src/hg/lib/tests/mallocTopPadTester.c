/* mallocTopPadTester - check that hg.conf's mallocTopPad still reaches the C library.
 *
 * cfgSetMallocTopPad() asks glibc to grow the heap in steps of mallocTopPad bytes instead of
 * its 128 kB default.  hgTracks loads tracks in parallel threads and each one grows its own
 * pool, so the small default step costs a heavy render more than a hundred thousand system
 * calls that draw nothing.  refs #38225.
 *
 * This is a performance setting, so the failure it has to catch is backsliding: the mallopt
 * call is dropped in a later edit, or the setting is renamed, or it stops being read before
 * the first allocation, and nothing anywhere goes red.  Every page still draws correctly and
 * the only symptom is that renders are slower than they were, which nobody attributes to this.
 *
 * So the test measures the step, not the clock.  A timing test would be worse than useless
 * here: it would be red on a busy hgwdev, green on an idle one, and deleted within a month.
 * The step size is deterministic.  sbrk(0) reports where the heap ends, and the distance it
 * jumps when malloc has to extend it IS what M_TOP_PAD sets, so the same two runs give the
 * same two answers on an idle machine and a loaded one.
 *
 * Read in buckets rather than exact byte counts, because glibc is entitled to round, to add
 * its own bookkeeping, and to change both between versions.  What must not change is the
 * order of magnitude between a configured step and the default one.
 *
 * Allocations stay under the 128 kB mmap threshold on purpose.  A larger request is served by
 * mmap instead of by extending the heap, and then the break never moves and the test measures
 * nothing while looking like it passes.
 *
 * refs #38225 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include <unistd.h>
#include "common.h"
#include "hgConfig.h"

/* Under the mmap threshold, so every one of these comes off the heap. */
#define CHUNK (64*1024)

/* Enough tries to reach past whatever the heap already had spare, and few enough that a run
 * that is measuring nothing ends rather than hanging. */
#define MAX_TRIES 4096

static long firstHeapJump()
/* Allocate small blocks until the top of the heap moves, and return how far it moved.  The
 * first move is the one that carries the step size; later ones are served out of what that
 * first one took. */
{
char *before = sbrk(0);
int i;
for (i = 0;  i < MAX_TRIES;  ++i)
    {
    char *block = needLargeMem(CHUNK);
    block[0] = 1;                 /* touch it, so nothing can optimise the request away */
    char *after = sbrk(0);
    if (after > before)
        return (long)(after - before);
    }
return 0;
}

static char *bucket(long bytes)
/* Name the order of magnitude.  An exact count would pin glibc's rounding and its
 * bookkeeping, neither of which this setting controls. */
{
if (bytes == 0)
    return "the heap never grew -- this run measured nothing";
if (bytes < 4L*1024*1024)
    return "under 4 MB, the library's own default step";
if (bytes < 16L*1024*1024)
    return "between 4 and 16 MB";
return "16 MB or more, a configured step";
}

int main(int argc, char *argv[])
{
/* Before the first allocation, exactly as the CGIs call it in main(). */
char *configured = cfgOption("mallocTopPad");
printf("mallocTopPad in hg.conf: %s\n", configured ? configured : "(not set)");
cfgSetMallocTopPad();

long jump = firstHeapJump();
printf("first heap growth:       %s\n", bucket(jump));
return 0;
}
