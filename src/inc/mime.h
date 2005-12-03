/*****************************************************************************
 * This file is copyright 2005 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. 
 *****************************************************************************/
/* mime.h - parses MIME messages, especially from a cgi from a multipart web form */

#ifndef HASH_H
#include "hash.h"
#endif 

#define MIMEBUFSIZE 32*1024  /* size of buffer for mime input */

struct mimePart
/* structure for an element of a MIME (multipart) message */
    {
    struct mimePart *next; /* next (sibling) if is part of multipart */
    struct hash *hdr;      /* hash of part headers */
    off_t size;     /* determines if local mem or saved to tempfile */
      /* only one of the next 3 pointers will be non-null, and that is the type */
    char* data;     /* if size< MAXPARTSIZE and does not contain null */
    char* fileName; /* if size>=MAXPARTSIZE or data contains null */
    boolean binary; /* if contains 0 chars, cannot store as a c-string */
    struct mimePart *multi;/* points to head of child list if itself contains multiparts */
    };

struct mimeBuf
/* structure for buffering a MIME message during parsing */
    {
    int d;                  /* descriptor (file,socket,etc) */
    char buf[MIMEBUFSIZE];  /* actual buffer */
    char *i;                /* index into buffer, current location */
    char *eop;              /* end of part or -1 */
    char *boundary;         /* boundary pattern for marking end of mime part */
    int  blen;              /* boundary pattern length (strlen) */
    char *eod;              /* end of data = eoi-(blen-1) */
    char *eoi;              /* end of input or -1 */
    char *eom;              /* end of memory just buf+MIMEBUFSIZE */
    };

char *getMimeHeaderMainVal(char *header);
/* Parse a typical mime header line returning the first
 * main value up to whitespace, punctuation, or end. 
 * freeMem the returned string when done */

char *getMimeHeaderFieldVal(char *header, char *field);
/* Parse a typical mime header line looking for field=
 * and return the value which may be quoted.
 * freeMem the returned string when done */

struct mimeBuf * initMimeBuf(int d);
/* d is a descriptor for a file or socket or some other descriptor 
   that the MIME input can be read from. 
   Initializes the mimeBuf structure. */

struct mimePart *parseMultiParts(struct mimeBuf *b, char *altHeader);
/* This is a recursive function.  It parses multipart MIME messages.
   Data that are binary or too large will be saved in mimePart->filename
   otherwise saved as a c-string in mimePart->data.  If multipart,
   then first child is mimePart->child, subsequent sibs are in child->next.
   altHeader is a string of headers that can be fed in if the headers have
   already been read off the stream by an earlier process, i.e. apache.
 */
