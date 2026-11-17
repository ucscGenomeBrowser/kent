/* recTrackSets - read the list of recommended track sets for a database. */

/* Copyright (C) 2020 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hdb.h"
#include "hui.h"
#include "hgConfig.h"
#include "recTrackSets.h"

char *recTrackSetsFile(char *db)
/* Generate path to file specifying menu of recommended track sets.
 *      eg, DOCUMENT_ROOT/data/recTrackSets/recTrackSets.hg19.tab */
{
char *root = hDocumentRoot();
char buf[200];
safef(buf, sizeof buf, "%s/%s/%s.%s.%s",
        root, REC_TRACK_SETS_DATA_DIR, REC_TRACK_SETS_FILE, db, REC_TRACK_SETS_EXT);
return cloneString(buf);
}

boolean recTrackSetsEnabled(char *db)
/* Return TRUE if feature is available */
{
char *cfgEnabled = cfgOption("browser.recTrackSets");
return cfgEnabled && (sameString(cfgEnabled, "on") || sameString(cfgEnabled, "true")) &&
        fileExists(recTrackSetsFile(db));
}

struct recTrackSet *loadRecTrackSets(char *db)
/* Read from tab-sep file.  Return list or NULL if no track sets for this database */
{
struct recTrackSet *recTrackSet, *recTrackSets = NULL;
struct lineFile *lf = lineFileOpen(recTrackSetsFile(db), TRUE);
if (!lf)
    return NULL;
#define cols 4
char *row[cols];
while (lineFileNextRowTab(lf, row, cols))
    {
    AllocVar(recTrackSet);
    recTrackSet->label = cloneString(row[0]);
    recTrackSet->userName = cloneString(row[1]);
    recTrackSet->sessionName = cloneString(row[2]);
    recTrackSet->description = cloneString(row[3]);
    slAddHead(&recTrackSets, recTrackSet);
    }
slReverse(&recTrackSets);
lineFileClose(&lf);
return recTrackSets;
}
