/* hgNear.h - interfaces to plug columns into hgNear. */

#ifndef HGNEAR_H
#define HGNEAR_H

#ifndef CART_H
#include "cart.h"
#endif 

#ifndef JKSQL_H
#include "jksql.h"
#endif

struct column
/* A column in the big table. */
   {
   /* Data set during initialization and afterwards held constant 
    * that is guaranteed to be in each track.  */
   struct column *next;		/* Next column. */
   char *name;			/* Column name, not allocated here. */
   char *label;			/* Column label. */

   /* -- Methods -- */
   char *(*cellVal)(struct column *col, char *geneId, struct sqlConnection *conn);
   /* Get value of one cell as string.  FreeMem this when done. */

   void (*cellPrint)(struct column *col, char *geneId, struct sqlConnection *conn);
   /* Print one cell of this column. */

   /* -- Data that may be track-specific. -- */
   char *table;			/* Name of associated table. */
   char *keyField;		/* GeneId field in associated table. */
   char *valField;		/* Value field in associated table. */
   };

/* ---- global variables ---- */

extern struct cart *cart; /* This holds variables between clicks. */
extern char *database;	  /* Name of genome database - hg15, or the like. */
extern char *organism;	  /* Name of organism - mouse, human, etc. */
extern char *curGeneId;	  /* Identity of current gene. */
extern char *sortOn;	  /* Current sort strategy. */
extern int displayCount;  /* Number of items to display. */

/* ---- Some general purpose helper routines. ---- */

void hvPrintf(char *format, va_list args);
/* Print out some html. */

void hPrintf(char *format, ...);
/* Print out some html. */

/* ---- Some helper routines for column methods. ---- */

char *cellSimpleVal(struct column *col, char *geneId, struct sqlConnection *conn);
/* Get a field in a table defined by col->table, col->keyField, col->valField. */

void cellSimplePrint(struct column *col, char *geneId, struct sqlConnection *conn);
/* This just prints cellSimpleVal. */

void simpleMethods(struct column *col, char *table, char *key, char *val);
/* Set up the simplest type of methods for column. */


/* ---- Column method setters. ---- */
void accMethods(struct column *col);
/* Set up methods for accession column. */

void percentIdMethods(struct column *col);
/* Set up methods for percentage ID column. */

void eValMethods(struct column *col);
/* Set up methods for e value column. */

void bitScoreMethods(struct column *col);
/* Set up methods for bit score column. */

#endif /* HGNEAR_H */

