/* xp - A minimal non-verifying xml parser.  It's
 * stream oriented much like expat.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef XP_H
#define XP_H

#ifndef DYSTRING_H
#include "dystring.h"
#endif

struct xpStack
/* A context stack for parser. */
    {
    struct dyString *tag;   /* Name of tag. */
    struct dyString *text;  /* Contains text if any. */
    };

struct xp
/* A minimal non-verifying xml parser. */
    {
    struct xp *next;	/* Next in list. */
    struct xpStack *stack;		/* Current top of stack. */
    struct xpStack stackBuf[64];	/* Stack buffer and start of stack. */
    struct xpStack *stackBufEnd;	/* End of stack buffer. */
    struct dyString *attDyBuf[128];	/* Attribute buffer as dynamic string. */
    char *attBuf[128];    	        /* Attribute buffer as regular string. */
    struct dyString *endTag;		/* Buffer for an end tag. */
    void *userData;			/* Pointer to user's data structure. */
    void (*atStartTag)(void *userData, char *name, char **atts);  /* Got start and attributes. */
    void (*atEndTag)(void *userData, char *name, char *text);   /* Got end. */
    int (*read)(void *userData, char *buf, int bufSize);   /* Read some more. */
    char *fileName;			/* File name. */
    int lineIx;				/* Number of current line. */
    char inBuf[16*1024];		/* Text input buffer. */
    char *inBufEnd;			/* Pointer to end of input buffer. */
    char *in;				/* Next character to parse. */
    struct hash *symHash;		/* Hash of &gt; &lt; etc. */
    };

struct xp *xpNew(void *userData, 
   void (*atStartTag)(void *userData, char *name, char **atts),
   void (*atEndTag)(void *userData, char *name, char *text),
   int (*read)(void *userData, char *buf, int bufSize),
   char *fileName);
/* Form a new xp parser.  File name may be NULL - just used for
 * error reporting. */

void xpFree(struct xp **pXp);
/* Free up an xp parser. */

int xpLineIx(struct xp *xp);
/* Return current line number. */

char *xpFileName(struct xp *xp);
/* Return current file name. */

int xpReadFromFile(void *userData, char *buf, int bufSize);
/* This is a function you can pass to xpNew as a read handler.
 * It assumes you've passed an open FILE in as userData. */

void xpError(struct xp *xp, char *format, ...);
/* Output an error message with filename and line number included. */

boolean xpParseNext(struct xp *xp, char *tag);
/* Skip through file until get given tag.  Then parse out the
 * tag and all of it's children (calling atStartTag/atEndTag).
 * You can call this repeatedly to process all of a given tag
 * in file. */

void xpParse(struct xp *xp);
/* Parse from start tag to end tag.  Throw error if a problem.
 * Returns FALSE if nothing left to parse. */

#endif /* XP_H */

