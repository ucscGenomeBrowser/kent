/* indelShift.c - find the leftmost or rightmost possible location of an insertion/deletion
 * variant with ambiguous placement, e.g. 'AAA' -> 'AAAA' -- where is the extra A inserted? */

/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#ifndef INDELSHIFT_H
#define INDELSHIFT_H

#include "seqWindow.h"

enum indelShiftDirection { isdLeft, isdRight };

#define INDEL_SHIFT_NO_MAX -1

boolean indelShiftIsApplicable(int refLen, int altLen);
/* Return true if it makes sense to try to shift this variant, i.e. if it's pure ins or del. */

void indelShiftRangeForVariant(uint varStart, int refLen, int altLen,
                               uint *retStart, uint *retEnd);
/* Given a variant start position and the lengths of reference and alternate alleles,
 * set retStart and retEnd to a range around varStart suitable for shifting around.
 * Caller must truncate retEnd to sequence size if necessary. */

int indelShift(struct seqWindow *seqWindow, uint *pVarStart, uint *pVarEnd, char *alt,
               int maxShift, enum indelShiftDirection direction);
/* Shift variant as far left or right as possible in seqName, expanding seqWindow as necessary,
 * updating pVarStart and pVarEnd, and possibly rotating the bases in alt (if non-empty).
 * If maxShift > INDEL_SHIFT_NO_MAX, shift at most maxShift bases.
 * Return the number of bases by which we shifted, usually 0.
 * NOTES:
 * - This can modify all pointer params.  Recompute anything derived from them if result is not 0.
 * - seqWin must have the correct seqName for variant and ideally will already contain a
 *   sufficiently large region around variant.
 * - This works only on pure insertions or deletions; if original ref and alt have identical
 *   bases at the beginning and/or end, trim those before passing into this function. */

#endif /* INDELSHIFT_H */
