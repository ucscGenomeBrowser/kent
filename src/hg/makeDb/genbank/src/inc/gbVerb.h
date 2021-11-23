/* Verbose tracing. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef GBVERB_H
#define GBVERB_H

#include <stdarg.h>

extern unsigned gbVerbose;  /* verbose level */ 

void gbVerbInit(int level);
/* Set verbose level and initialize start time */

void gbVerbEnter(int level, char* msg, ...)
/* If the verbose level is level or greater, print message.  Increase
 * indentation amount. */ 
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;

void gbVerbLeave(int level, char* msg, ...)
/* If the verbose level is level or greater, print message.  Decrease
 * indentation amount.  Must be matched with gbVerbEnter */ 
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;

void gbVerbMsg(int level, char* msg, ...)
/* If the verbose level is level or greater, print message. */ 
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;

void gbVerbPr(int level, char* msg, ...)
/* If the verbose level is level or greater, print message, with no time
 * or resource info included. */ 
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;

void gbVerbPrStart(int level, char* msg, ...)
/* like gbVerbPr, only doesn't print a newline; use gbVerbPrMore
 * to add more lines and then the newline*/
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;

void gbVerbPrMore(int level, char* msg, ...)
/* output more after gbVerbPrStart, with no newline */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;


#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
