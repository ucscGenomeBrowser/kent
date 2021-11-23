/* unirefTbl - load and manage uniref data. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef UNIREFTBL_H
#define UNIREFTBL_H

#include "uniref.h"

struct unirefTbl *unirefTblNew(char *unrefTabFile, char *orgFilter);
/* construct a unirefTbl object from the tab seperated file.  If orgFilter is
 * not null, load only records for this organism */

void unirefTblFree(struct unirefTbl **utPtr);
/* free a unirefTbl object. */

struct uniref *unirefTblGetEntryById(struct unirefTbl *ut, char *entryId);
/* Get the uniref entry list (ptr to rep) for an entry id, or NULL */

struct uniref *unirefTblGetEntryByAcc(struct unirefTbl *ut, char *acc);
/* Get the entry list (ptr to rep) give any accession in the entry */

#endif
