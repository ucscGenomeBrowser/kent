/* xap - XML Automatic Parser - together with autoXml this helps automatically
 * read in automatically generated data structures. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef XAP_H
#define XAP_H

#ifndef DYSTRING_H
#include "dystring.h"
#endif

struct xapStack
/* An element of parse stack. */
   {
   void *object;	/* Object being parsed. */
   char *elName;	/* Name of tag. */
   struct dyString *text;	/* The text section. */
   };

struct xap
/* A parser with a stack built in and other conveniences. */
   {
   struct xapStack *stack;			/* Top of stack of open tags. */
   struct xapStack stackBuf[128];		/* Stack buffer. */
   struct xapStack *endStack;			/* End of stack. */
   int stackDepth;				/* Number of elements in stack. */
   int skipDepth;				/* Depth beneath which we skip. */
   void *(*startHandler)(struct xap *xap, char *name, char **atts);	/* Handler for start tags. */
   void (*endHandler)(struct xap *xap, char *name);	/* Handler for end tags. */
   struct xp *xp;				        /* Pointer to lower level parser. */
   char *fileName;				/* Name of file being parsed - valid after xapParse. */
   char *topType;				/* Top level type name - valid after xapParse. */
   void *topObject;				/* Top level object - valid after xapParse. */
   FILE *f;					/* File. */
   };

struct xap *xapNew(void *(*startHandler)(struct xap *xap, char *name, char **atts),
	void (*endHandler)(struct xap *xap, char *name) ,
	char *fileName);
/* Create a new parse stack.  FileName may be NULL. */

void xapFree(struct xap **pXp);
/* Free up a parse stack. */

void xapParse(struct xap *xap, char *fileName);
/* Open up file and parse it all. */

void xapError(struct xap *xap, char *format, ...);
/* Issue an error message and abort*/

void xapIndent(int count, FILE *f);
/* Write out some spaces. */

void xapSkip(struct xap *xap);
/* Skip current tag and any children.  Called from startHandler. */

void xapParseAny(char *fileName, char *type, 
	void *(*startHandler)(struct xap *xap, char *name, char **atts),
	void (*endHandler)(struct xap *xap, char *name),
	char **retType, void *retObj);
/* Parse any object out of an XML file. 
 * If type parameter is non-NULL, force type.
 * example:
 *     xapParseAny("file.xml", "das", dasStartHandler, dasEndHandler, &type, &obj); */

#endif /* XAP_H */

