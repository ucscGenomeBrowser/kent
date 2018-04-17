/* seqWindow -- generic interface & implementations for fetching subranges of a sequence */

/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hdb.h"
#include "seqWindow.h"

static void seqWindowUpdateRangeAndSeq(struct seqWindow *seqWin, char *seqName,
                                       uint start, uint end, char *seq)
/* Update seqWin's window coordinates and sequence -- all fetch methods need to do this.
 * seq is *not* cloned, don't free it afterward! */
{
if (!sameOk(seqName, seqWin->seqName))
    {
    freeMem(seqWin->seqName);
    seqWin->seqName = cloneString(seqName);
    }
seqWin->start = start;
seqWin->end = end;
freeMem(seqWin->seq);
if (seq)
    touppers(seq);
seqWin->seq = seq;
}

static void seqWindowFreeShared(struct seqWindow **pSw)
/* Free the parts of seqWindow that are shared by all implementations.
 * This *does not* zero out *pSw like a normal Free would, so that implementations
 * can free up the implementation-specific parts of *pSw */
{
if (pSw && *pSw)
    {
    struct seqWindow *sw = (struct seqWindow *)*pSw;
    freeMem(sw->seqName);
    freeMem(sw->seq);
    }
}

struct chromSeqWindow
/* seqWindow for chrom sequence from a genome database */
{
    struct seqWindow sw;   // generic interface
    char *db;              // db from which this can fetch sequence
    };

#define CHROMSEQ_CACHE_FUDGE 4096

static void chromSeqFetch(struct seqWindow *seqWin, char *chrom, uint start, uint end)
/* seqWindow fetch method for updating window with new location & sequence if window does not
 * already cover the requested location. */
{
struct chromSeqWindow *csw = (struct chromSeqWindow *)seqWin;
boolean sameChrom = sameOk(seqWin->seqName, chrom);
if (!sameChrom || start < seqWin->start || end > seqWin->end)
    {
    // We must fetch new sequence. Expand range by CHROMSEQ_CACHE_FUDGE so if we get
    // successive requests for nearby sequences, we won't have to fetch sequence as often.
    int chromSize = hChromSize(csw->db, chrom);
    if (start > chromSize)
        errAbort("chromSeqFetch: start (%u) is out of range for %s %s (length %d)",
                 start, csw->db, chrom, chromSize);
    uint bufStart = (start > CHROMSEQ_CACHE_FUDGE) ? start - CHROMSEQ_CACHE_FUDGE : 0;
    uint bufEnd = end + CHROMSEQ_CACHE_FUDGE;
    // Tolerate & clip ranges that extend past the end of the sequence
    if (bufEnd > chromSize)
        bufEnd = chromSize;
    struct dnaSeq *dnaSeq = hChromSeq(csw->db, chrom, bufStart, bufEnd);
    if (dnaSeq)
        {
        bufEnd = bufStart + dnaSeq->size;  // should be unnecessary but just in case
        seqWindowUpdateRangeAndSeq(seqWin, chrom, bufStart, bufEnd, dnaSeqCannibalize(&dnaSeq));
        }
    else
        {
        // No sequence for chrom
        errAbort("chromSeqFetch: unable to get sequence for %s [%d,%d)", chrom, start, end);
        }
    }
}

struct seqWindow *chromSeqWindowNew(char *db, char *chrom, uint start, uint end)
/* Return a new seqWindow that can fetch uppercase sequence from the chrom sequences in db.
 * If chrom is non-NULL and end > start then load sequence from that range; if chrom is non-NULL
 * and start == end == 0 then fetch entire chrom. */
{
struct chromSeqWindow *csw;
AllocVar(csw);
csw->sw.fetch = chromSeqFetch;
csw->db = cloneString(db);
if (start > end)
    errAbort("chromSeqWindowNew: start (%u) should be <= end (%u)", start, end);
if (chrom != NULL)
    chromSeqFetch((struct seqWindow *)csw, chrom, start, end);
return (struct seqWindow *)csw;
}

void chromSeqWindowFree(struct seqWindow **pSw)
/* Free a chromSeqWindow. */
{
if (pSw && *pSw)
    {
    seqWindowFreeShared(pSw);
    struct chromSeqWindow *csw = (struct chromSeqWindow *)*pSw;
    freeMem(csw->db);
    freez(pSw);
    }
}

static void memSeqFetch(struct seqWindow *seqWin, char *acc, uint start, uint end)
/* No changes, we have what we have. */
{
if (!sameOk(seqWin->seqName, acc))
    errAbort("memSeqFetch: sequence name '%s' requested from window on '%s'", acc, seqWin->seqName);
}

struct seqWindow *memSeqWindowNew(char *acc, char *seq)
/* Return a new seqWindow copying this sequence already in memory. */
{
struct seqWindow *sw;
AllocVar(sw);
sw->seqName = cloneString(acc);
sw->seq = cloneString(seq ? seq : "");
touppers(sw->seq);
sw->start = 0;
sw->end = strlen(sw->seq);
sw->fetch = memSeqFetch;
return sw;
}

void memSeqWindowFree(struct seqWindow **pSw)
/* Free a seqWindow that was created by memSeqWindowNew. */
{
if (pSw && *pSw)
    {
    seqWindowFreeShared(pSw);
    // No extra stuff for memSeqWindow
    freez(pSw);
    }
}

struct twoBitSeqWindow
/* seqWindow for twoBit file */
    {
    struct seqWindow sw;     // generic interface
    struct twoBitFile *tbf;  // twoBitFile from which this can fetch sequence
    };

#define TWOBITSEQ_CACHE_FUDGE 4096

static void twoBitSeqFetch(struct seqWindow *seqWin, char *chrom, uint start, uint end)
/* seqWindow fetch method for updating window with new location & sequence if window does not
 * already cover the requested location. */
{
struct twoBitSeqWindow *tsw = (struct twoBitSeqWindow *)seqWin;
boolean sameChrom = sameOk(seqWin->seqName, chrom);
if (!sameChrom || start < seqWin->start || end > seqWin->end)
    {
    // We must fetch new sequence. Expand range by CHROMSEQ_CACHE_FUDGE so if we get
    // successive requests for nearby sequences, we won't have to fetch sequence as often.
    int chromSize = twoBitSeqSize(tsw->tbf, chrom);
    if (start > chromSize)
        errAbort("twoBitSeqFetch: start (%u) is out of range for %s %s (length %d)",
                 start, tsw->tbf->fileName, chrom, chromSize);
    if (start == 0 && end == 0)
        end = chromSize;
    uint bufStart = (start > CHROMSEQ_CACHE_FUDGE) ? start - CHROMSEQ_CACHE_FUDGE : 0;
    uint bufEnd = end + CHROMSEQ_CACHE_FUDGE;
    // Tolerate & clip ranges that extend past the end of the sequence
    if (bufEnd > chromSize)
        bufEnd = chromSize;
    struct dnaSeq *dnaSeq = twoBitReadSeqFragLower(tsw->tbf, chrom, bufStart, bufEnd);
    if (dnaSeq)
        {
        bufEnd = bufStart + dnaSeq->size;  // should be unnecessary but just in case
        seqWindowUpdateRangeAndSeq(seqWin, chrom, bufStart, bufEnd, dnaSeqCannibalize(&dnaSeq));
        }
    else
        {
        // No sequence for chrom
        errAbort("twoBitSeqFetch: unable to get sequence for %s [%d,%d)", chrom, start, end);
        }
    }
}

struct seqWindow *twoBitSeqWindowNew(char *twoBitFileName, char *chrom, uint start, uint end)
/* Return a new seqWindow that can fetch uppercase sequence from twoBitFileName.
 * If chrom is non-NULL and end > start then load sequence from that range; if chrom is non-NULL
 * and start == end == 0 then fetch entire chrom. */
{
struct twoBitSeqWindow *tsw;
AllocVar(tsw);
tsw->sw.fetch = twoBitSeqFetch;
tsw->tbf = twoBitOpen(twoBitFileName);
if (start > end)
    errAbort("twoBitSeqWindowNew: start (%u) should be <= end (%u)", start, end);
if (chrom != NULL)
    twoBitSeqFetch((struct seqWindow *)tsw, chrom, start, end);
return (struct seqWindow *)tsw;
}

void twoBitSeqWindowFree(struct seqWindow **pSw)
/* Free a twoBitSeqWindow. */
{
if (pSw && *pSw)
    {
    seqWindowFreeShared(pSw);
    struct twoBitSeqWindow *tsw = (struct twoBitSeqWindow *)*pSw;
    twoBitClose(&tsw->tbf);
    freez(pSw);
    }
}
