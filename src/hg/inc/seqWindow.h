/* seqWindow -- generic interface & implementations for fetching subranges of a sequence */

/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#ifndef SEQWINDOW_H
#define SEQWINDOW_H

struct seqWindow
/* A portion (possibly all) of a sequence, with a means to fetch some other portion of sequence. */
    {
    char *seqName;         // Name of the sequence on which the current window is open.
    uint start;            // Start within seqName of current window.
    uint end;              // End within seqName of current window.
    char *seq;             // Uppercase IUPAC sequence of current window.

    void (*fetch)(struct seqWindow *self, char *seqName, uint start, uint end);
    /* Generic method to set the window to a new range and get a new chunk of uppercase sequence.
     * The sequence in seqWindow after fetching may be a larger range than what was requested,
     * so caller must not assume that the resulting start and end are same as requested.
     * If end is too large then it will be truncated to sequence size.
     * errAbort if unable to get sequence. */

    // Implementations hide state/details after this point.
    };

INLINE void seqWindowCopy(struct seqWindow *self, uint start, uint len, char *buf, size_t bufSize)
/* Copy len bases of sequence into buf, starting at seqName coord start; errAbort if out of range.
 * Zero-terminate buf and errAbort if bufSize < len+1. */
{
uint end = start + len;
if (start >= self->start && end <= self->end)
    safencpy(buf, bufSize, self->seq + start - self->start, len);
else
    errAbort("seqWindowCopy: %s [%u,%u) is out of bounds [%u,%u)",
             self->seqName, start, end, self->start, self->end);
}

struct seqWindow *chromSeqWindowNew(char *db, char *chrom, uint start, uint end);
/* Return a new seqWindow that can fetch uppercase sequence from the chrom sequences in db.
 * If chrom is non-NULL and end > start then load sequence from that range; if chrom is non-NULL
 * and start == end == 0 then fetch entire chrom. */

void chromSeqWindowFree(struct seqWindow **pSw);
/* Free a seqWindow that was created by chromSeqWindowNew. */

struct seqWindow *memSeqWindowNew(char *acc, char *seq);
/* Return a new seqWindow copying this sequence already in memory. */

void memSeqWindowFree(struct seqWindow **pSw);
/* Free a seqWindow that was created by memSeqWindowNew. */

struct seqWindow *twoBitSeqWindowNew(char *twoBitFileName, char *chrom, uint start, uint end);
/* Return a new seqWindow that can fetch uppercase sequence from twoBitFileName.
 * If chrom is non-NULL and end > start then load sequence from that range; if chrom is non-NULL
 * and start == end == 0 then fetch entire chrom. */

void twoBitSeqWindowFree(struct seqWindow **pSw);
/* Free a seqWindow that was created by twoBitSeqWindowNew. */

#endif /* SEQWINDOW2_H */
