/* cdwVcfFileFromRa - fill in a cdwVcfFile from a .ra file.  This file largely 
 * generated automatically with raToStructGen. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CDWVCFFILEFROMRA_H

struct raToStructReader *cdwVcfFileRaReader();
/* Make a raToStructReader for cdwVcfFile */

struct cdwVcfFile *cdwVcfFileFromNextRa(struct lineFile *lf, struct raToStructReader *reader);
/* Return next stanza put into an cdwVcfFile. */

struct cdwVcfFile *cdwVcfFileOneFromRa(char *fileName);
/* Return list of all cdwVcfFiles in file. */

#endif /* CDWVCFFILEFROMRA_H */
