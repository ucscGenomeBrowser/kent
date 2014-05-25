/*  Last edited: Apr 26 14:37 1994 (rd) */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

extern int readSequence (FILE *fil, int *conv,
			 char **seq, char **id, char **desc, int *length) ;
				/* read next sequence from file */
extern int seqConvert (char *seq, int *length, int *conv) ;
				/* convert in place - can shorten */
extern int dna2textConv[] ;
extern int dna2indexConv[] ;
