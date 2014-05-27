/* customPp - custom track preprocessor.  This handles the line-oriented
 * input/output for the custom track system.  The main services it provides
 * are taking care of references to URLs, which are treated as includes,
 * and moving lines that start with "browser" to a list.
 *
 * Also this allows unlimited "pushBack" of lines, which is handy, since
 * the system will analyse a number of lines of a track to see what format
 * it's in. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CUSTOMPP_H
#define CUSTOMPP_H

struct customPp
/* Custom track preprocessor. */
    {
    struct customPp *next;	 /* Next customPp in list */
    struct lineFile *fileStack;  /* Keep track of files we're included from */
    struct slName *browserLines; /* Lines seen so far that start w/ browser */
    struct slName *reusedLines;  /* Lines pushed back by customPpReuse. */
    struct slName *inReuse;	 /* Line in process of being reused. */
    struct slName *skippedLines; /* Nonempty lines skipped by most recent customPpNextReal */
    boolean ignoreBrowserLines;  /* Flag to suppress removal of browser lines */
                                 /*   so preprocessor can be used with docs */
#ifdef PROGRESS_METER
    off_t remoteFileSize;	/* reported size from URL */
#endif
    };

struct customPp *customPpNew(struct lineFile *lf);
/* Return customPp on lineFile */

void customPpFree(struct customPp **pCpp);
/* Close files and free up customPp. */

char *customPpNext(struct customPp *cpp);
/* Return next line. */

char *customPpNextReal(struct customPp *cpp);
/* Return next line that's nonempty and non-space. */

void customPpReuse(struct customPp *cpp, char *line);
/* Reuse line.  May be called many times before next customPpNext/NextReal.
 * Should be called with last line to be reused first if called multiply. */

struct slName *customPpTakeBrowserLines(struct customPp *cpp);
/* Grab browser lines from cpp, which will no longer have them. */

struct slName *customPpCloneSkippedLines(struct customPp *cpp);
/* Return a clone of most recent nonempty skipped (comment/header) lines from cpp,
 * which will still have them.  slFreeList when done. */

struct customPp *customDocPpNew(struct lineFile *lf);
/* Return customPp for doc file that leaves browser lines */

char *customPpFileName(struct customPp *cpp);
/* Return the name of the current file being parsed (top of fileStack).  Free when done. */

#endif /* CUSTOMPP_H */

