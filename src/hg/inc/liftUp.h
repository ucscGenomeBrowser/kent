/* Copyright (C) 2003 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef LIFTUP_H
#define LIFTUP_H

struct liftSpec
/* How to lift coordinates. */
    {
    struct liftSpec *next;	/* Next in list. */
    int offset;			/* Offset to add. */
    char *oldName;		/* Name in source file. */
    int oldSize;                /* Size of old sequence. */
    char *newName;		/* Name in dest file. */
    int newSize;                   /* Size of new sequence. */
    char strand;                /* Strand of contig relative to chromosome. */
    };

struct liftSpec *readLifts(char *fileName);
/* Read in lift file. */

struct hash *hashLift(struct liftSpec *list, boolean revOk);
/* Return a hash of the lift spec.  If revOk, allow - strand elements.  */

#endif /* LIFTUP_H */

