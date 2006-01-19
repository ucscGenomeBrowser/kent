/* hgKgGetText - Get text from known genes into a file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "spDb.h"

static char const rcsid[] = "$Id: hgKgGetText.c,v 1.1 2006/01/19 16:32:27 kent Exp $";

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
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct kgInfo
/* Basic known gene info. */
    {
    struct kgInfo *next;	/* Next in list. */
    char *name;			/* Known gene id. */
    char *swissProt;		/* Swiss Prot display id. */
    };

struct kgInfo *getKgList(struct sqlConnection *conn)
/* Get list of all known genes. */
{
struct hash *uniqHash = hashNew(16);
struct kgInfo *kgList = NULL, *kg;
char **row;
/* struct sqlResult *sr = sqlGetResult(conn, "select name,proteinId from knownGene limit 100"); */
struct sqlResult *sr = sqlGetResult(conn, "select name,proteinId from knownGene");

while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    AllocVar(kg);
    if (!hashLookup(uniqHash, name))
	{
	hashAdd(uniqHash, name, kg);
	kg->name = cloneString(row[0]);
	kg->swissProt = cloneString(row[1]);
	slAddHead(&kgList, kg);
	}
    }
sqlFreeResult(&sr);
slReverse(&kgList);
hashFree(&uniqHash);
return kgList;
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
safef(query, sizeof(query), "select %s from %s where %s='%s'", 
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

void getText(struct kgInfo *kg, struct sqlConnection *conn, 
	struct sqlConnection *spConn, struct sqlConnection *goConn,
	FILE *f)
/* Get loads of text and write it out. */
{
char query[512], **row;
struct sqlResult *sr;
struct hash *uniqHash = hashNew(0);
char *spAcc = spFindAcc(spConn, kg->swissProt);

fprintf(f, "%s", kg->name);
addSimple(kg->name, "kgAlias", "kgID", "alias", conn, uniqHash, f);
addSimple(kg->name, "kgProtAlias", "kgID", "alias", conn, uniqHash, f);
addSimple(kg->name, "kgXref", "kgID", "description", conn, uniqHash, f);

safef(query, sizeof(query),
    "select refSeqSummary.summary from kgXref,refSeqSummary "
    "where kgXref.kgID='%s' and refSeqSummary.mrnaAcc=kgXref.refseq"
    , kg->name);
addText(query, conn, f);

safef(query, sizeof(query),
    "select commentVal.val from comment,commentVal "
    "where comment.acc='%s' and comment.commentVal=commentVal.id"
    , spAcc);
addText(query, spConn, f);

safef(query, sizeof(query),
    "select term.name from goaPart,term "
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
struct kgInfo *kgList = NULL, *kg;

kgList = getKgList(conn);
verbose(1, "Read in %d known genes from %s\n", slCount(kgList), database);

for (kg = kgList; kg != NULL; kg = kg->next)
    getText(kg, conn, spConn, goConn, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hgKgGetText(argv[1], argv[2]);
return 0;
}
