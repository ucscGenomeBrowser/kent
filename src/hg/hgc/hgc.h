/* hgc - Human Genome Click processor - gets called when user clicks
 * on something in human tracks display. This file contains stuff
 * shared with other modules in hgc,  but not in other programs. */

#ifndef CART_H
#include "cart.h"
#endif

extern struct cart *cart;	/* User's settings. */
extern char *seqName;		/* Name of sequence we're working on. */
extern int winStart, winEnd;    /* Bounds of sequence. */
extern char *database;		/* Name of mySQL database. */

void hgcStart(char *title);
/* Print out header of web page with title.  Set
 * error handler to normal html error handler. */

char *hgcPath();
/* Return path of this CGI script. */

char *hgcPathAndSettings();
/* Return path with this CGI script and session state variable. */

void hgcAnchorSomewhere(char *group, char *item, char *other, char *chrom);
/* Generate an anchor that calls click processing program with item 
 * and other parameters. */

void hgcAnchorWindow(char *group, char *item, int thisWinStart, 
        int thisWinEnd, char *other, char *chrom);
/* Generate an anchor that calls click processing program with item
 * and other parameters, INCLUDING the ability to specify left and
 * right window positions different from the current window*/

void hgcAnchor(char *group, char *item, char *other);
/* Generate an anchor that calls click processing program with item 
 * and other parameters. */

boolean clipToChrom(int *pStart, int *pEnd);
/* Clip start/end coordinates to fit in chromosome. */

void printCappedSequence(int start, int end, int extra);
/* Print DNA from start to end including extra at either end.
 * Capitalize bits from start to end. */

void printPos(char *chrom, int start, int end, char *strand, boolean featDna);
/* Print position lines.  'strand' argument may be null. */

void bedPrintPos(struct bed *bed, int bedSize);
/* Print first three fields of a bed type structure in
 * standard format. */

void genericHeader(struct trackDb *tdb, char *item);
/* Put up generic track info. */

void printTrackHtml(struct trackDb *tdb);
/* If there's some html associated with track print it out. */
