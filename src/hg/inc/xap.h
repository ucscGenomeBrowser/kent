/* xap - XML Automatic Parser - together with autoXml this helps automatically
 * read in automatically generated data structures. */

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
   void *(*startHandler)(struct xap *xp, char *name, char **atts);	/* Handler for start tags. */
   void (*endHandler)(struct xap *xp, char *name);	/* Handler for end tags. */
   void *expat;				        /* Pointer to expat parser. */
   char *fileName;				/* Name of file being parsed - valid after xapParse. */
   char *topType;				/* Top level type name - valid after xapParse. */
   void *topObject;				/* Top level object - valid after xapParse. */
   };

struct xap *xapNew(void *(*startHandler)(struct xap *xp, char *name, char **atts),
	void (*endHandler)(struct xap *xp, char *name) );
/* Create a new parse stack. */

void xapFree(struct xap **pXp);
/* Free up a parse stack. */

void xapParse(struct xap *xp, char *fileName);
/* Open up file and parse it all. */

void xapError(struct xap *xp, char *format, ...);
/* Issue an error message and abort*/

void xapIndent(int count, FILE *f);
/* Write out some spaces. */

void xapSkip(struct xap *xp);
/* Skip current tag and any children.  Called from startHandler. */

void xapParseAny(char *fileName, char *type, 
	void *(*startHandler)(struct xap *xp, char *name, char **atts),
	void (*endHandler)(struct xap *xp, char *name),
	char **retType, void *retObj);
/* Parse any object out of an XML file. 
 * If type parameter is non-NULL, force type.
 * example:
 *     xapParseAny("file.xml", "das", dasStartHandler, dasEndHandler, &type, &obj); */

#endif /* XAP_H */

