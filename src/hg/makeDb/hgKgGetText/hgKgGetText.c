/* hgKgGetText - Get text from known genes into a file. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "kgXref.h"
#include "spDb.h"


char *summaryTable = "hgFixed.refSeqSummary";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgKgGetText - Get text from known genes into a file.\n"
  "The file will be line oriented with the known gene ID as\n"
  "the first word, and the rest of the word being a conglomaration\n"
  "of all descriptive text for known genes.\n"
  "usage:\n"
  "   hgKgGetText database out.txt\n"
  "options:\n"
  "   summaryTable=table Use given table to get summary info, default %s"
  , summaryTable
  );
}

boolean gotRefSeqSummary;

static struct optionSpec options[] = {
   {"summaryTable", OPTION_STRING},
   {NULL, 0},
};

struct kgXref *getKgList(struct sqlConnection *conn)
/* Get list of all known genes. */
{
/* Verify that the number of fields present in this kgXref table is what's
 * expected, since more fields were added to the schema recently (10/19/2011) */
struct slName *kgXrefFields = sqlListFields(conn, "kgXref");
if (slCount(kgXrefFields) != KGXREF_NUM_COLS) 
    {
    errAbort("This genome has %d columns in kgXref but %d are expected - old genome?", 
	     slCount(kgXrefFields), KGXREF_NUM_COLS);
    }
slFreeList(kgXrefFields);

struct sqlResult *sr = sqlGetResult(conn, NOSQLINJ "select * from kgXref");
struct kgXref *kgList = NULL, *kg;
char **row;
struct hash *uniqHash = hashNew(18);

while ((row = sqlNextRow(sr)) != NULL)
    {
    kg = kgXrefLoad(row);
    if (hashLookup(uniqHash, kg->kgID) == NULL)
	{
	hashAdd(uniqHash, kg->kgID, NULL);
	slAddHead(&kgList, kg);
	}
    }
sqlFreeResult(&sr);
slReverse(&kgList);
hashFree(&uniqHash);
return kgList;
}

struct hash *getRefSeqSummary(struct sqlConnection *conn)
/* Return hash keyed by refSeq NM_ id, with description values. */
{
struct hash *hash = hashNew(16);
char query[256];
sqlSafef(query, sizeof(query), "select mrnaAcc,summary from %s", summaryTable);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    subChar(row[1], '\n', ' ');
    hashAdd(hash, row[0], cloneString(row[1]));
    }
sqlFreeResult(&sr);
verbose(1, "%d %s elements\n", hash->elCount, summaryTable);
return hash;
}


void addText(char *query, struct sqlConnection *conn, FILE *f)
/* Add results of query to line.  Convert newlines to spaces. */
{
char **row;
struct sqlResult *sr;

fprintf(f, "\t");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *val = row[0];
    subChar(val, '\n', ' ');
    fprintf(f, "%s ", val);
    }
sqlFreeResult(&sr);
}

void addSimple(char *id, char *table, char *idField, char *valField, 
	struct sqlConnection *conn, struct hash *uniqHash, FILE *f)
/* Append all values associated with ID in table to line of file. */
{
char query[256], **row;
struct sqlResult *sr;

fprintf(f, "\t");
sqlSafef(query, sizeof(query), "select %s from %s where %s='%s'", 
    valField, table, idField, id);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *val = row[0];
    if (!hashLookup(uniqHash, val))
        {
	hashAdd(uniqHash, val, NULL);
	fprintf(f, "%s ", val);
	}
    }
sqlFreeResult(&sr);
}

void getText(struct kgXref *kg, struct hash *refSeqHash, 
	struct sqlConnection *conn, 
	struct sqlConnection *spConn, struct sqlConnection *goConn,
	FILE *f)
/* Get loads of text and write it out. */
{
char query[512];
struct hash *uniqHash = hashNew(0);
char *spAcc = spFindAcc(spConn, kg->spID);

subChar(kg->description, '\n', ' ');
fprintf(f, "%s\t%s\t%s\t%s", kg->kgID, 
	kg->geneSymbol, kg->kgID, kg->description);
hashAdd(uniqHash, kg->geneSymbol, NULL);
hashAdd(uniqHash, kg->kgID, NULL);
addSimple(kg->kgID, "kgAlias", "kgID", "alias", conn, uniqHash, f);
addSimple(kg->kgID, "kgProtAlias", "kgID", "alias", conn, uniqHash, f);

if (refSeqHash != NULL)
    {
    char *s = hashFindVal(refSeqHash, kg->refseq);
    if (s == NULL && strchr(kg->refseq, '.') != NULL)
	{
	char *accOnly = cloneString(kg->refseq);
	chopSuffix(accOnly);
	s = hashFindVal(refSeqHash, accOnly);
	freeMem(accOnly);
	}
    if (s == NULL)
        s = "";
    fprintf(f, "\t%s", s);
    }

safef(query, sizeof(query),
    "NOSQLINJ select commentVal.val from comment,commentVal "
    "where comment.acc='%s' and comment.commentVal=commentVal.id"
    , spAcc);
addText(query, spConn, f);

safef(query, sizeof(query),
    "NOSQLINJ select term.name from goaPart,term "
    "where goaPart.dbObjectId='%s' "
    "and goaPart.goId=term.acc"
    , spAcc);
addText(query, goConn, f);

fprintf(f, "\n");
hashFree(&uniqHash);
freeMem(spAcc);
}

void hgKgGetText(char *database, char *outFile)
/* hgKgGetText - Get text from known genes into a file. */
{
FILE *f = mustOpen(outFile, "w");
struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *spConn = sqlConnect("uniProt");
struct sqlConnection *goConn = sqlConnect("go");
struct kgXref *kgList = NULL, *kg;
struct hash *refSeqHash = NULL;
/* Return hash keyed by refSeq NM_ id, with description values. */

gotRefSeqSummary = sqlTableExists(conn, summaryTable);
if (gotRefSeqSummary)
    refSeqHash = getRefSeqSummary(conn);
else
    warn("No %s table in %s, proceeding without...", summaryTable, database);
kgList = getKgList(conn);
verbose(1, "Read in %d known genes from %s\n", slCount(kgList), database);

for (kg = kgList; kg != NULL; kg = kg->next)
    getText(kg, refSeqHash, conn, spConn, goConn, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
summaryTable = optionVal("summaryTable", summaryTable);
hgKgGetText(argv[1], argv[2]);
return 0;
}
