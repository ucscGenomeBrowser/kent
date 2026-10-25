/* exonFramePhasesTester - the exon mouseover reads no further than the end of exonFrames.
 *
 * The exon mouseover gives the codon phase at each end of an exon, and the end phase is the
 * frame of the next exon along the transcript.  On the last exon of a forward-strand
 * transcript there is no next exon, and before #38309 the read went one element past the end
 * of exonFrames.  The value read was thrown away, so nothing on the page showed it; valgrind
 * did.  The reverse-strand branch already stopped at its own end.
 *
 * Here exonFrames ends at the last byte of a page and the page after it is unreadable, so a
 * read past the end kills the tester instead of passing unseen.
 *
 * refs #38309 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "genePred.h"
#include "hgTracks.h"
#include <sys/mman.h>
#include <unistd.h>

static int *framesBeforeGuard(int *frames, int count)
/* Return a copy of frames that ends at a page boundary, with an unreadable page after it. */
{
long pageSize = sysconf(_SC_PAGESIZE);
char *base = mmap(NULL, 2 * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);
if (base == MAP_FAILED)
    errnoAbort("mmap");
if (mprotect(base + pageSize, pageSize, PROT_NONE) != 0)
    errnoAbort("mprotect");
int *copy = (int *)(base + pageSize) - count;
memcpy(copy, frames, count * sizeof(int));
return copy;
}

static void showTranscript(char *what, boolean revStrand)
/* Print the phases of every exon of a three-exon transcript. */
{
int frames[] = {0, 2, 1};
struct genePred gp;
ZeroVar(&gp);
gp.exonCount = ArraySize(frames);
gp.exonFrames = framesBeforeGuard(frames, gp.exonCount);
printf("%s\n", what);
int exonIx;
for (exonIx = 1;  exonIx <= gp.exonCount;  ++exonIx)
    {
    int startPhase = -1, endPhase = -1;
    exonFramePhases(&gp, exonIx, revStrand, &startPhase, &endPhase);
    printf("  exon %d  start phase %2d  end phase %2d\n", exonIx, startPhase, endPhase);
    }
}

int main(int argc, char *argv[])
{
showTranscript("forward strand, exonFrames 0 2 1", FALSE);
showTranscript("reverse strand, exonFrames 0 2 1", TRUE);
return 0;
}
