/* spKgMap - map swissprot accs to known gene ids */
#include "common.h"
#include "spKgMap.h"
#include "jksql.h"
#include "kgXref.h"
#include "kgProtAlias.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"

static char const rcsid[] = "$Id: spKgMap.c,v 1.3 2006/07/06 21:08:00 fanhsu Exp $";

struct spKgMap
/* map of swissprot accs to kg ids */
{
    struct hash *spHash;  /* hash to slName list, or to NULL if no mapping */
};

static void spKgMapAdd(struct spKgMap *spKgMap, char *spAcc, char *kgId)
/* add an entry to the map, if it doesn't already exist. */
{
struct hashEl *spHel = hashStore(spKgMap->spHash, spAcc);

if ((spHel->val == NULL) || !slNameInList(spHel->val, kgId))
    {
    struct slName *kgIdSln = lmSlName(spKgMap->spHash->lm, kgId);
    slAddHead((struct slName**)&spHel->val, kgIdSln);
    }
}

static void loadKgXRef(struct spKgMap *spKgMap,
                       struct sqlConnection *conn)
/* load kgXRef data into the table */
{
char query[1024];
struct kgXref kgXref;
struct sqlResult *sr;
char **row;

safef(query, sizeof(query), "SELECT * from kgXref");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    kgXrefStaticLoad(row, &kgXref);
    spKgMapAdd(spKgMap, kgXref.spID, kgXref.kgID);
    }
sqlFreeResult(&sr);
}

static void loadKgProtAlias(struct spKgMap *spKgMap,
                            struct sqlConnection *conn)
/* load entries from kgProtAlias that appear to be for other 
 * swissprot.  Just looks for aliases starting with O,P, or Q.
 * followed by a number. */
{
char query[1024];
struct kgProtAlias kgPA;
struct sqlResult *sr;
char **row;

safef(query, sizeof(query), "SELECT * from kgProtAlias");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    kgProtAliasStaticLoad(row, &kgPA);
    if ((strlen(kgPA.alias) > 2)
        && ((kgPA.alias[0] == 'O') || (kgPA.alias[0] == 'P') || (kgPA.alias[0] ==  'Q'))
        && isdigit(kgPA.alias[1]))
        spKgMapAdd(spKgMap, kgPA.alias, kgPA.kgID);
    }
sqlFreeResult(&sr);
}

static void addSpSecondaryAcc(struct spKgMap *spKgMap, char *spAcc, char *spAcc2)
/* add a secondary accession to the map */
{
struct slName *kgId = spKgMapGet(spKgMap, spAcc);
if (kgId != NULL)
    spKgMapAdd(spKgMap, spAcc2, kgId->name);
}

static void loadSpSecondaryAcc(struct spKgMap *spKgMap)
/* Add secondary accessions from swissProt database, link */
{
struct sqlConnection *conn = sqlConnect("swissProt");
char query[1024];
struct sqlResult *sr;
char **row;

safef(query, sizeof(query), "SELECT acc,val FROM otherAcc");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    addSpSecondaryAcc(spKgMap, row[0], row[1]);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

struct spKgMap *spKgMapNew(struct sqlConnection *conn)
/* load data from from kgXRef and kgProtAlias tables to map from swissprot
 * accs to known gene ids. */
{
struct spKgMap *spKgMap;

AllocVar(spKgMap);
spKgMap->spHash = hashNew(24);
loadKgXRef(spKgMap, conn);
loadKgProtAlias(spKgMap, conn);
loadSpSecondaryAcc(spKgMap);
return spKgMap;
}

void spKgMapFree(struct spKgMap **spKgMapPtr)
/* free a spKgMap structure */
{
struct spKgMap *spKgMap = *spKgMapPtr;
if (spKgMap != NULL)
    {
    hashFree(&spKgMap->spHash);
    freeMem(spKgMap);
    *spKgMapPtr = NULL;
    }
}

struct slName *spKgMapGet(struct spKgMap *spKgMap, char *spAcc)
/* Get the list of known gene ids for a swissprot acc.  If not found NULL is
 * returned. */
{
return hashFindVal(spKgMap->spHash, spAcc);
}

