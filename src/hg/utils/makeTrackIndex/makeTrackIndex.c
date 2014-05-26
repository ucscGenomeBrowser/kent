/* Make a track ixIxx file for the given to be used in the track search tool. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "options.h"
#include "hdb.h"
#include "grp.h"
#include "hui.h"
#include "ra.h"
#include "jsHelper.h"
#include "mdb.h"
#include "cv.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeTrackIndex - make a ixIxx input file for the tracks in given assembly.\n"
  "usage:\n"
  "   makeTrackIndex database metaDbTableName cv.ra\n\n");
}

static void printTrack(struct sqlConnection *conn, struct trackDb *tdb, char *html,
                       struct hash *grpHash, struct hash *tdbHash, struct hash *mdbObjHash)
{
// Don't duplicate superTracks
if (hashLookup(tdbHash,tdb->track))
    return;
hashAdd(tdbHash,tdb->track,tdb);

char *metadataList[] = {"longLabel", NULL}; // include a fixed set of metadata (i.e. not stuff like subGroup).
int i;
char *grpLabel, *tmp;

printf("%s ", tdb->track);
printf("%s ", tdb->shortLabel);
struct mdbObj *mdbObj = mdbObjLookUp(mdbObjHash,tdb->track);
struct mdbVar *var = NULL;
if (mdbObj != NULL)
    {
    // vals for all mdb vars or just searchable vars?
    var = mdbObj->vars;
    for ( ; var != NULL; var = var->next)
        {
        if (cvSearchMethod(var->var) != cvNotSearchable)
            printf("%s ", var->val);
        }
    }

// We include the long descriptions for these terms, b/c we assume people will search for the long labels.
// Jim want's cv data after short/long label

// cv descriptions should be after mdb values.
if (mdbObj != NULL)
    {
    // vals for all mdb vars or just searchable vars?
    var = mdbObj->vars;
    for ( ; var != NULL; var = var->next)
        {
        if (cvTermIsCvDefined(var->var) // If it isn't cvDefined then the fields aren't there
        &&  cvSearchMethod(var->var) != cvNotSearchable)
            {
            struct hash *hash = (struct hash *)cvOneTermHash(var->var,var->val);
            if(hash != NULL)
                {
                // hand-curated list of which cv.ra fields to include in fulltext index.
                char *fields[] = {"description", "targetDescription", NULL};
                int ix;
                for(ix = 0; fields[ix] != NULL; ix++)
                    {
                    char *str = hashFindVal(hash, fields[ix]);
                    if (str)
                        printf("%s ", str);
                    }
                }
            }
        }
    }

for(i = 0; metadataList[i] != NULL; i++)
    {
    char *str = trackDbLocalSetting(tdb, metadataList[i]);
    if(str != NULL)
        printf("%s ", str);
    }

grpLabel = tdb->grp;
tmp = hashFindVal(grpHash, grpLabel);
if(tmp != NULL)
    grpLabel = tmp;
printf("%s ", grpLabel);


if(html == NULL)
    {
    html = tdb->html;
    if(html != NULL)
        // strip out html markup
        html = stripRegEx(html, "<[^>]*>", REG_ICASE);
    }
if(html != NULL)
    {
    char *tmp = cloneString(html);
    char *val = nextWord(&tmp);
    while(val != NULL)
        {
        printf("%s ", val);
        val = nextWord(&tmp);
        }
    }
printf("\n");

if (tdbIsSuper(tdb->parent))
    printTrack(conn, tdb->parent, NULL, grpHash, tdbHash, mdbObjHash);
if(tdb->subtracks != NULL)
    {
    struct trackDb *subtrack;
    for (subtrack = tdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
        // We have decided NOT to populate html down to children.
        printTrack(conn, subtrack, NULL, grpHash, tdbHash, mdbObjHash);
    }
}

int main(int argc, char *argv[])
{
struct trackDb *tdb, *tdbList = NULL;
struct hash *grpHash = newHash(0);
struct hash *tdbHash = newHash(0);
struct grp *grp, *grps;
char filePath[PATH_LEN];

if (argc != 4)
    usage();
char *database = argv[1];
char *metaDbName = argv[2];
char *cvRaPath = argv[3];
tdbList = hTrackDb(database);
struct sqlConnection *conn = hAllocConn(database);

grps = hLoadGrps(database);
for (grp = grps; grp != NULL; grp = grp->next)
    hashAdd(grpHash, grp->name, grp->label);

safecpy(filePath, sizeof(filePath), cvRaPath );
if(!fileExists(filePath))
    errAbort("Error: can't locate cv.ra; %s doesn't exist\n", filePath);
cvFileDeclare(filePath);

struct hash *mdbObjHash = NULL;
struct mdbObj *mdbObjs = mdbObjsQueryAll(conn,metaDbName);
if (mdbObjs != NULL)
    mdbObjHash = mdbObjsHash(mdbObjs);

for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    printTrack(conn, tdb, NULL, grpHash, tdbHash, mdbObjHash);

// Overkill:
//mdbObjsHashFree(&mdbObjHash);
//mdbObjsFree(&mdbObjs);

return 0;
}
