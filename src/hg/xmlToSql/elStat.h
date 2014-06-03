/* Parse a statistics file from autoDtd. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef ELSTAT_H
#define ELSTAT_H

struct attStat
/* Statistics on an xml attribute. */
   {
   struct attStat *next;
   char *name;		/* Attribute name. */
   int maxLen;		/* Maximum length. */
   char *type;		/* int/float/string/none */
   int count;		/* Number of times attribute seen. */
   int uniqCount;	/* Number of unique values of attribute. */
   };

struct elStat
/* Statistics on an xml element. */
   {
   struct elStat *next;	/* Next in list. */
   char *name;		/* Element name. */
   int count;		/* Number of times element seen. */
   struct attStat *attList;	/* List of attributes. */
   };

struct elStat *elStatLoadAll(char *fileName);
/* Read all elStats from file. */

#endif /* ELSTAT_H */
