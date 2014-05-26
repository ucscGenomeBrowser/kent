/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


#ifndef BLACKLIST_H
#define BLACKLIST_H

/*
 * accession black list routines used by genbank pipeline and
 * pslCDnaFilter.  A black list file is a set of accession ranges.
 * It is a two columned table with the first field the being the
 * beginning of the range, and the second field being the end of
 * the range (inclusive)
 * Version numbers are ignored (by using atoi)
 */

struct blackListRange  /* a range of extensions with a common prefix */
{
struct blackListRange *next;
char *prefix;  /* prefix to accession number (eg. FV) */
int begin;     /* starting value to range */
int end;       /* ending value to range */
};

struct blackListRange *genbankBlackListParse(char *blackList);
/* parse a black list file into blackListRange data structure */

boolean genbankBlackListFail(char *accession, struct blackListRange *ranges);
/* check to see if accession is black listed */

#endif /* BLACKLIST_H */
