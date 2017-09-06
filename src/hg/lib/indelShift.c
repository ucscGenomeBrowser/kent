/* indelShift.c - find the leftmost or rightmost possible location of an insertion/deletion
 * variant with ambiguous placement, e.g. 'AAA' -> 'AAAA' -- where is the extra A inserted? */

/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dnaseq.h"
#include "indelShift.h"

boolean indelShiftIsApplicable(int refLen, int altLen)
/* Return true if it makes sense to try to shift this variant, i.e. if it's pure ins or del. */
{
return ((refLen == 0 && altLen > 0) ||  // insertion into reference
        (refLen > 0 && altLen == 0));   // deletion from reference
}

void indelShiftRangeForVariant(uint varStart, int refLen, int altLen,
                                 uint *retStart, uint *retEnd)
/* Given a variant start position and the lengths of reference and alternate alleles,
 * set retStart and retEnd to a range around varStart suitable for shifting around.
 * Caller must truncate retEnd to sequence size if necessary. */
{
int padding = max(128, 2*refLen+altLen);
*retStart = (varStart > padding) ? varStart - padding : 0;
*retEnd = varStart + refLen + altLen + padding;
}

static void expandWindowIfNecessary(struct seqWindow *seqWin, uint varStart, int refLen, int altLen)
/* Make sure there's enough padding to start a sliding-window search around variant. */
{
uint maxStart, minEnd;
indelShiftRangeForVariant(varStart, refLen, altLen, &maxStart, &minEnd);
seqWin->fetch(seqWin, seqWin->seqName, maxStart, minEnd);
}

static int fetchLeft(struct seqWindow *seqWin, int basesShifted)
/* We need to fetch more sequence to the left; update seqWin and p* params
 * with the distance that we have shifted so far, and fetch a larger block of sequence.
 * Return the new offset of the variant start within seqWin. */
{
// Double the size of our padding on the left (but don't underflow seq)
int padding = basesShifted * 2;
if (padding > seqWin->start)
    padding = seqWin->start;
if (padding > 0)
    {
    uint newStart = seqWin->start - padding;
    seqWin->fetch(seqWin, seqWin->seqName, newStart, seqWin->end);
    }
return padding;
}

static uint slideLeftOne(struct seqWindow *seqWin, uint start, int *pBasesShifted)
/* Advance one base to the left; if we hit the beginning of the sequence window, fetch more. */
{
uint newStart = start - 1;
*pBasesShifted += 1;
if (newStart == 0)
    newStart = fetchLeft(seqWin, *pBasesShifted);
return newStart;
}

INLINE boolean notMax(int basesShifted, int maxShift)
/* Return TRUE if we have not yet hit the limit or if there is no limit. */
{
return (maxShift <= INDEL_SHIFT_NO_MAX) || (basesShifted < maxShift);
}

static int slideLeft(struct seqWindow *seqWin, uint varStart, int refLen, char *alt, int maxShift)
/* Slide left within seqWin as far as we can; if we still haven't found a mismatch then
 * fetch more sequence and keep going.  Return the number of bases shifted.
 * This can modify all params (except maxShift). */
{
int basesShifted = 0;
int altLen = strlen(alt);
// Relative coords within seqWin:
uint start = varStart - seqWin->start;
boolean done = FALSE;
// If insertion, first compare inserted seq to genomic seq to the left of insertion point.
int ix;
for (ix = altLen-1;  !done && ix >= 0 && start > 0 && notMax(basesShifted, maxShift);  ix--)
    {
    if (seqWin->seq[start-1] == alt[ix])
        start = slideLeftOne(seqWin, start, &basesShifted);
    else
        done = TRUE;
    }
// If we're not done, keep trying to shift left.
while (!done && start > 0 && notMax(basesShifted, maxShift))
    {
    if (seqWin->seq[start-1] == seqWin->seq[start-1+refLen+altLen])
        start = slideLeftOne(seqWin, start, &basesShifted);
    else
        done = TRUE;
    }
return basesShifted;
}

static void fetchRight(struct seqWindow *seqWin, int extraPadding, int basesShifted)
/* We need to fetch more sequence to the right; update seqWin and p* params
 * with the distance that we have shifted so far, and fetch a larger block of sequence.
 * Return the number of bases by which we expanded to the right. */
{
// Double the size of our padding on the right
int padding = basesShifted * 2 + extraPadding;
uint newEnd = seqWin->end + padding;
seqWin->fetch(seqWin, seqWin->seqName, seqWin->start, newEnd);
}

static uint slideRightOne(struct seqWindow *seqWin, uint start, int refLen, int altLen,
                          int *pBasesShifted)
/* Advance one base to the right; if we hit the end of the sequence window, fetch more. */
{
uint newStart = start + 1;
*pBasesShifted += 1;
if (newStart >= seqWin->end - seqWin->start - refLen - altLen)
    fetchRight(seqWin, refLen+altLen, *pBasesShifted);
return newStart;
}

static int slideRight(struct seqWindow *seqWin, uint varStart, int refLen, char *alt, int maxShift)
/* Slide right within seqWin as far as we can; if we still haven't found a mismatch then
 * fetch more sequence and keep going.  Return the number of bases shifted.
 * This can modify all params (except maxShift). */
{
int basesShifted = 0;
int altLen = strlen(alt);
// Relative coords within seqWin:
uint start = varStart - seqWin->start;
boolean done = FALSE;
// If insertion, first compare inserted seq to genomic seq to the right of insertion point.
int ix;
for (ix = 0; !done && ix < altLen && start < seqWin->end - seqWin->start - refLen &&
             notMax(basesShifted, maxShift); ix++)
    {
    if (seqWin->seq[start] == alt[ix])
        start = slideRightOne(seqWin, start, refLen, altLen, &basesShifted);
    else
        done = TRUE;
    }
// If we're not done, keep trying to shift right.
while (!done && start < seqWin->end - seqWin->start - refLen && notMax(basesShifted, maxShift))
    {
    if (seqWin->seq[start - altLen] == seqWin->seq[start + refLen])
        start = slideRightOne(seqWin, start, refLen, altLen, &basesShifted);
    else
        done = TRUE;
    }
return basesShifted;
}

int indelShift(struct seqWindow *seqWin, uint *pVarStart, uint *pVarEnd, char *alt,
               int maxShift, enum indelShiftDirection direction)
/* Shift variant as far left or right as possible in seqName, expanding seqWindow as necessary,
 * updating pVarStart and pVarEnd, and possibly rotating the bases in alt (if non-empty).
 * If maxShift > INDEL_SHIFT_NO_MAX, shift at most maxShift bases.
 * Return the number of bases by which we shifted, usually 0.
 * NOTES:
 * - This can modify all params (except direction).  Recompute anything derived from them
 *   if result is not 0.
 * - seqWin must have the correct seqName for variant and ideally will already contain a
 *   sufficiently large region around variant.
 * - This works only on pure insertions or deletions; if original ref and alt have identical
 *   bases at the beginning and/or end, trim those before passing into this function. */
{
int basesShifted = 0;
int refLen = *pVarEnd - *pVarStart;
if (refLen < 0)
    errAbort("indelShift: variant end (%u) is less than variant start (%u)",
             *pVarEnd, *pVarStart);
int altLen = strlen(alt);
if (indelShiftIsApplicable(refLen, altLen))
    {
    // It's a pure insertion or deletion; attempt to slide the variant left
    expandWindowIfNecessary(seqWin, *pVarStart, refLen, altLen);
    if (direction == isdLeft)
        basesShifted = slideLeft(seqWin, *pVarStart, refLen, alt, maxShift);
    else if (direction == isdRight)
        basesShifted = slideRight(seqWin, *pVarStart, refLen, alt, maxShift);
    else
        errAbort("indelShift: invalid direction (%d), should be isdLeft or isdRight", direction);
    if (basesShifted)
        {
        // Update coords and alt sequence (if non-empty)
        int coordOffset = (direction == isdRight) ? basesShifted : -basesShifted;
        *pVarStart += coordOffset;
        *pVarEnd += coordOffset;
        if (altLen > 0)
            {
            if (basesShifted >= altLen)
                // All of alt is a shifted portion of the reference.
                seqWindowCopy(seqWin, *pVarStart - (direction == isdLeft ? 0 : altLen), altLen,
                              alt, altLen+1);
            else if (direction == isdRight)
                {
                // The beginning of alt is a left-shifted portion of the original.  The remainder
                // is from reference seq.  Do the shifting first, then the copying from ref.
                int shiftedLen = altLen - basesShifted;
                memmove(alt, alt+basesShifted, shiftedLen);
                seqWindowCopy(seqWin, *pVarStart - basesShifted, basesShifted,
                              alt+shiftedLen, basesShifted+1);
                }
            else
                {
                // The beginning of alt is from reference seq. The remainder is a right-shifted
                // portion of the original.  Do the shifting first, then the copying from ref.
                memmove(alt+basesShifted, alt, (altLen - basesShifted));
                // Note: can't use seqWindowCopy because it zero-terminates.
                char *refAlStart = seqWin->seq + (*pVarStart - seqWin->start);
                memcpy(alt, refAlStart, basesShifted);
                }
            }
        }
    }
return basesShifted;
}

