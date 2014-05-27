/* edwBamFileFromRa - fill in a edwBamFile from a .ra file.  This file largely 
 * generated automatically with raToStructGen. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef EDWBAMFILEFROMRA_H

struct raToStructReader *edwBamFileRaReader();
/* Make a raToStructReader for edwBamFile */

struct edwBamFile *edwBamFileFromNextRa(struct lineFile *lf, struct raToStructReader *reader);
/* Return next stanza put into an edwBamFile. */

struct edwBamFile *edwBamFileOneFromRa(char *fileName);
/* Return list of all edwBamFiles in file. */

#endif /* EDWBAMFILEFROMRA_H */
