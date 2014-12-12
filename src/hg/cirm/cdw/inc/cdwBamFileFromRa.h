/* cdwBamFileFromRa - fill in a cdwBamFile from a .ra file.  This file largely 
 * generated automatically with raToStructGen. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CDWBAMFILEFROMRA_H

struct raToStructReader *cdwBamFileRaReader();
/* Make a raToStructReader for cdwBamFile */

struct cdwBamFile *cdwBamFileFromNextRa(struct lineFile *lf, struct raToStructReader *reader);
/* Return next stanza put into an cdwBamFile. */

struct cdwBamFile *cdwBamFileOneFromRa(char *fileName);
/* Return list of all cdwBamFiles in file. */

#endif /* CDWBAMFILEFROMRA_H */
