/* cdwFastqFileFromRa - fill in a cdwFastqFile from a .ra file.  This file largely 
 * generated automatically with raToStructGen. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CDWFASTQFILEFROMRA_H

struct raToStructReader *cdwFastqFileRaReader();
/* Make a raToStructReader for cdwFastqFile */

struct cdwFastqFile *cdwFastqFileFromNextRa(struct lineFile *lf, struct raToStructReader *reader);
/* Return next stanza put into an cdwFastqFile. */

struct cdwFastqFile *cdwFastqFileOneFromRa(char *fileName);
/* Return list of all cdwFastqFiles in file. */

#endif /* CDWFASTQFILEFROMRA_H */
