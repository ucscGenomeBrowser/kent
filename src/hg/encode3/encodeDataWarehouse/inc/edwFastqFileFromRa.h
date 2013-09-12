/* edwFastqFileFromRa - fill in a edwFastqFile from a .ra file.  This file largely 
 * generated automatically with raToStructGen. */

#ifndef EDWFASTQFILEFROMRA_H

struct raToStructReader *edwFastqFileRaReader();
/* Make a raToStructReader for edwFastqFile */

struct edwFastqFile *edwFastqFileFromNextRa(struct lineFile *lf, struct raToStructReader *reader);
/* Return next stanza put into an edwFastqFile. */

struct edwFastqFile *edwFastqFileOneFromRa(char *fileName);
/* Return list of all edwFastqFiles in file. */

#endif /* EDWFASTQFILEFROMRA_H */
