/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

/* handlerList - a list of handler functions for translating identifiers
 * from different external databases to browser positions.  Used by
 * linkInHandlers.c. */

#include "common.h"
#include "hdb.h"
#include "jksql.h"
#include "handlerList.h"

/********************************************
 *      UniProtKB Handler
 *******************************************/

/* Databases whose IDs we can link to using the position parameter.  ... this varies by database,
 * so be restrictive.  Ordered by preference. */
/* 1 = EMBL, 2 = RefSeq, */
#define UniProtExtRefSeqId 2
#define UniProtExtEMBLId 1

/*
 * +---------+--------------+----+-----+------+--+
 * |     acc |     char(12) | NO | MUL | NULL |  |
 * |   extDb |      int(11) | NO |     | NULL |  |
 * | extAcc1 | varchar(255) | NO | MUL | NULL |  |
 * | extAcc2 | varchar(255) | NO |     | NULL |  |
 * | extAcc3 | varchar(255) | NO |     | NULL |  |
 * +---------+--------------+----+-----+------+--+
 *
 * Looks like UniProt marks the end of the IDs list on
 * each line with a '-'.  Supplementary material may follow
 * in the extra field(s), e.g.
 * P01892  1       M86404  -       NOT_ANNOTATED_CDS
 */


void addIdsToList(struct slName **listPtr, char **ids, int count)
/* Given a list of alternate identifiers taken from the uniProt
 * table, add them to the supplied list.  The identifiers list is
 * terminated with a '-' */
{
int i=0;
for (i=0; i<count; i++)
    {
    if (sameString(ids[i], "-"))
        break;
    slNameStore(listPtr, ids[i]);
    }
}


char *uniProtAccToDb(char *id, struct sqlConnection *conn)
/* Searches the uniProt database (which conn must be connected to)
 * for the given identifier and returns the name of the database
 * that is most relevant.  This is usually the default assembly
 * for the genome that the identifier is on. */
{
char *db = NULL;
char query[4096];
sqlSafef(query, sizeof(query), "select taxon from accToTaxon where acc = '%s'", id);
int taxon = sqlQuickNum(conn, query);
if (taxon != 0)
    db = hDbForTaxon(taxon);
return db;
}


struct linkInResult *getEmblList(char *db, struct slName *emblNames, struct sqlConnection *conn)
/* Given a connection to a genome database (e.g., hg19), the name of that database, and
 * a list of identifiers, search for any EMBL mRNA names that we have positions for.
 * Return a list of matching positions. */
{
struct linkInResult *results = NULL;
bool hasMRna = sqlTableExists(conn, "all_mrna");
if (!hasMRna)
    return NULL;
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "select tName, tStart, tEnd from all_mrna where qName in (");
struct slName *thisName = emblNames;
bool firstName = TRUE;
while (thisName != NULL)
    {
    if (firstName)
        {
        sqlDyStringPrintfFrag(query, "'%s'", thisName->name);
        firstName = FALSE;
        }
    else
        sqlDyStringPrintfFrag(query, ",'%s'", thisName->name);
    thisName = thisName->next;
    }
sqlDyStringPrintfFrag(query, ")");
struct sqlResult *sr = sqlGetResult(conn, dyStringContents(query));
char **row = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct linkInResult *newResult = NULL;
    AllocVar(newResult);
    newResult->db = cloneString(db);
    char position[4096];
    safef(position, sizeof(position), "%s:%s-%s", row[0], row[1], row[2]);
    newResult->position = cloneString(position);
    newResult->trackToView = cloneString("all_mrna");
    slAddHead(&results, newResult);
    }
slReverse(&results);
sqlFreeResult(&sr);
return results;
}


struct linkInResult *getRefGeneList(char *db, struct slName *refNames, struct sqlConnection *conn)
/* Given a connection to a genome database (e.g., hg19), the name of that database, and
 * a list of identifiers, search for any RefSeq gene names that we have positions for.
 * Return a list of matching positions. */
{
struct linkInResult *results = NULL;
bool hasNcbiRefGene = sqlTableExists(conn, "ncbiRefGene");
bool hasRefGene = sqlTableExists(conn, "refGene");
if (!hasNcbiRefGene && !hasRefGene)
    return NULL;
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "select chrom, txStart, txEnd from %s where name in (",
        hasNcbiRefGene?"ncbiRefGene":"refGene");
struct slName *thisName = refNames;
bool firstName = TRUE;
while (thisName != NULL)
    {
    if (firstName)
        {
        sqlDyStringPrintfFrag(query, "'%s'", thisName->name);
        firstName = FALSE;
        }
    else
        sqlDyStringPrintfFrag(query, ",'%s'", thisName->name);
    thisName = thisName->next;
    }
sqlDyStringPrintfFrag(query, ")");
struct sqlResult *sr = sqlGetResult(conn, dyStringContents(query));
char **row = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct linkInResult *newResult = NULL;
    AllocVar(newResult);
    newResult->db = cloneString(db);
    char position[4096];
    safef(position, sizeof(position), "%s:%s-%s", row[0], row[1], row[2]);
    newResult->position = cloneString(position);
    newResult->trackToView = cloneString(hasNcbiRefGene?"ncbiRefGene":"refGene");
    slAddHead(&results, newResult);
    }
slReverse(&results);
sqlFreeResult(&sr);
return results;
}


struct linkInResult *uniProtHandler(struct sqlConnection *conn, char *id)
/* Handler for translating a UniProtKB identifier to a genome assembly and
 * positions within that assembly.  Changes the database that conn
 * points to - first to uniProt, then to the relevant assembly.
 * Looks first for related RefSeq gene positions.  If none are found, will
 * then search for related mRNA positions. */
{
char query[8092], **row = NULL;
struct slName *emblIdList = NULL, *refSeqIdList = NULL;

sqlSafef(query, sizeof(query), "use uniProt");
sqlUpdate(conn, query);
char *db = uniProtAccToDb(id, conn);
if (isEmpty(db))
    return NULL;

sqlSafef(query, sizeof(query),
        "select extDb, extAcc1, extAcc2, extAcc3 from extDbRef where "
        "acc = '%s' and extDb in (%d, %d)", id, UniProtExtRefSeqId, UniProtExtEMBLId);
/* Assemble resulting IDs into two lists; one for refSeq, and one for EMBL */
struct sqlResult *sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int extDbId = atoi(row[0]);
    switch(extDbId)
        {
        case UniProtExtEMBLId:
            addIdsToList(&emblIdList, &row[1], 3);
            break;
        case UniProtExtRefSeqId:
            addIdsToList(&refSeqIdList, &row[1], 3);
            break;
        default:
            errAbort("Got unexpected external database ID %d", extDbId);
        }
    }
sqlFreeResult(&sr);

sqlSafef(query, sizeof(query), "use %s", db);
sqlUpdate(conn, query);
struct linkInResult *searchResults = NULL;
/* If there are any RefSeq IDs, and if they're in our database, use that */
if (refSeqIdList != NULL)
    searchResults = getRefGeneList(db, refSeqIdList, conn);

/* otherwise, hunt for any EMBL hits */
if ((searchResults == NULL) && (emblIdList != NULL))
    searchResults = getEmblList(db, emblIdList, conn);

return searchResults;
}

/***************************************
 *      End of UniProtKB handler
 **************************************/
