/* hgKnownMore - Create the knownMore table from a variety of sources.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "knownMore.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "hugoMulti.h"
#include "knownInfo.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgKnownMore - Create the knownMore table from a variety of sources.\n"
  "usage:\n"
  "   hgKnownMore database omim_to_transcript.map omimIds nomeids.txt\n"
  "where omim_to_transcript.map comes from Affymetrix and connect genie predictions to\n"
  "OMIM ids, omimIds connects names and omimIds and is from NCBI, and nomeids \n"
  "has lots of information and is from HUGO Gene Nomenclature Committee.\n"
  );
}

void readHugoMultiTable(char *fileName, struct hugoMulti **retList,
	struct hash **retIdHash, struct hash **retSymbolHash)
/* Read in file into list and hashes.  Make hash keyed on omim ID
 * and on OMIM symbol.  */
{
struct hash *idHash = newHash(0);
struct hash *symbolHash = newHash(0);
struct hugoMulti *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[16];
char *line;
int lineSize, wordCount;
char *name;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == 0 || line[0] == '#')
        continue;
    wordCount = chopTabs(line, words);
    lineFileExpectWords(lf, 11, wordCount);
    el = hugoMultiLoad(words);
    slAddHead(&list, el);
    name = el->omimId;
    if (name[0] != 0)
	hashAdd(idHash, name, el);
    name = el->symbol;
    if (name[0] != 0)
	hashAdd(symbolHash, name, el);
    }
lineFileClose(&lf);
slReverse(&list);
*retList = list;
*retIdHash = idHash;
*retSymbolHash = symbolHash;
}


struct nameOmim
/* Connect name with OMIM ID */
    {
    struct nameOmim *next;	/* Next in list. */
    char *name;                 /* Symbolic name. */
    char *omimId;		/* OMIM numerical ID as a string. */
    };

void readNameOmim(char *fileName, struct nameOmim **retList, struct hash **retNameOmimHash,
	struct hash **retOmimNameHash)
/* Read in file into list and hashes.  Make hash keyed on transcriptId (txOmimHash)
 * and hash keyed on omimId (omimNameHash). */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *nameOmimHash = newHash(0);
struct hash *omimNameHash = newHash(0);
struct nameOmim *list = NULL, *el;
char *row[2];

while (lineFileRow(lf, row))
    {
    AllocVar(el);
    slAddHead(&list, el);
    touppers(row[0]);
    hashAddSaveName(nameOmimHash, row[0], el, &el->name);
    hashAddSaveName(omimNameHash, row[1], el, &el->omimId);
    }
lineFileClose(&lf);
slReverse(&list);
*retList = list;
*retNameOmimHash = nameOmimHash;
*retOmimNameHash = omimNameHash;
}


struct txOmim
/* Connect transcriptID with OMIM ID */
    {
    struct txOmim *next;	/* Next in list. */
    char *transId;              /* Genie transcript ID. */
    char *omimId;		/* OMIM ID as a string. */
    };

void readTxOmim(char *fileName, struct txOmim **retList, struct hash **retTxOmimHash,
	struct hash **retOmimTxHash)
/* Read in file into list and hashes.  Make hash keyed on transcriptId (txOmimHash)
 * and hash keyed on omimId (omimTxHash). */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *txOmimHash = newHash(0);
struct hash *omimTxHash = newHash(0);
struct txOmim *list = NULL, *el;
char *row[2];
char *omim;

while (lineFileRow(lf, row))
    {
    AllocVar(el);
    slAddHead(&list, el);
    hashAddSaveName(txOmimHash, row[0], el, &el->transId);
    omim = row[1];
    if (!startsWith("omim:", omim))
        errAbort("Expecting 'omim:' at start of second field line %d of %s",
	    lf->lineIx, lf->fileName);
    omim += 5;
    hashAddSaveName(omimTxHash, omim, el, &el->omimId);
    }
lineFileClose(&lf);
slReverse(&list);
*retList = list;
*retTxOmimHash = txOmimHash;
*retOmimTxHash = omimTxHash;
}

struct knownInfo *loadKnownInfo(struct sqlConnection *conn)
/* Load up known info table into list. */
{
struct knownInfo *list = NULL, *el;
struct sqlResult *sr;
char **row;

sr = sqlGetResult(conn, NOSQLINJ "select * from knownInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = knownInfoLoad(row);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

void hgKnownMore(char *database, char *txOmimMap, char *omimIds, char *nomeIds)
/* hgKnownMore - Create the knownMore table from a variety of sources.. */
{ 
struct hash *txOmimHash = NULL, *omimTxHash = NULL;
struct txOmim *txOmimList = NULL, *txOmim;
struct hash *nameOmimHash = NULL, *omimNameHash = NULL;
struct nameOmim *nameOmimList = NULL, *nameOmim;
struct hash *hmOmimHash = NULL, *hmSymbolHash = NULL;
struct hugoMulti *hmList = NULL, *hm;
struct knownInfo *kiList = NULL, *ki;
struct knownMore km;
struct sqlConnection *conn;
char *tabName = "knownMore.tab";
FILE *f = NULL;
char *omimIdString = NULL;
char query[256];

readHugoMultiTable(nomeIds, &hmList, &hmOmimHash, &hmSymbolHash);
printf("Read %d elements in %s\n", slCount(hmList), nomeIds);
readNameOmim(omimIds, &nameOmimList, &nameOmimHash, &omimNameHash);
printf("Read %d elements in %s\n", slCount(nameOmimList), omimIds);
readTxOmim(txOmimMap, &txOmimList, &txOmimHash, &omimTxHash);
printf("Read %d elements in %s\n", slCount(txOmimList), txOmimMap);

conn = sqlConnect(database);
kiList = loadKnownInfo(conn);
printf("Read %d elements from knownInfo table in %s\n", slCount(kiList), database);

printf("Writing %s\n", tabName);
f = mustOpen(tabName, "w");
for (ki = kiList; ki != NULL; ki = ki->next)
    {
    /* Fill out a knownMore data structure.  Start with all zero
     * just to avoid garbage. */
    zeroBytes(&km, sizeof(km));

    /* First fields come from knownInfo generally. */
    km.name = ki->name;	/* The name displayed in the browser: OMIM, gbGeneName, or transId */
    km.transId = ki->transId;	/* Transcript id. Genie generated ID. */
    km.geneId = ki->geneId;	/* Gene (not transcript) Genie ID */
    km.gbGeneName = ki->geneName;	/* Connect to geneName table. Genbank gene name */
    km.gbProductName = ki->productName;	/* Connects to productName table. Genbank product name */
    km.gbProteinAcc = ki->proteinId;	/* Genbank accession of protein */
    km.gbNgi = ki->ngi;	/* Genbank gi of nucleotide seq. */
    km.gbPgi = ki->pgi;	/* Genbank gi of protein seq. */

    /* Fill in rest with acceptable values for no-data-present. */
    km.omimId = 0;	/* OMIM ID or 0 if none */
    km.omimName = "";	/* OMIM primary name */
    km.hugoId = 0;	/* HUGO Nomeclature Committee ID or 0 if none */
    km.hugoSymbol = "";	/* HUGO short name */
    km.hugoName = "";	/* HUGO descriptive name */
    km.hugoMap = "";	/* HUGO Map position */
    km.pmId1 = 0;	/* I have no idea - grabbed from a HUGO nomeids.txt */
    km.pmId2 = 0;	/* Likewise, I have no idea */
    km.refSeqAcc = "";	/* Accession of RefSeq mRNA */
    km.aliases = "";	/* Aliases if any.  Comma and space separated list */
    km.locusLinkId = 0;	/* Locus link ID */
    km.gdbId = "";	/* NCBI GDB database ID */

    /* See if it's a disease gene with extra info. */
    txOmim = hashFindVal(txOmimHash, km.transId);
    if (txOmim != NULL)
	{
	omimIdString = txOmim->omimId;
	km.omimId = atoi(omimIdString);	/* OMIM ID or 0 if none */
	nameOmim = hashFindVal(omimNameHash, omimIdString);
	if (nameOmim != NULL)
	    {
	    km.name = km.omimName = nameOmim->name;
	    }
	hm = hashFindVal(hmOmimHash, omimIdString);
	if (hm != NULL)
	    {
	    km.hugoId = hm->hgnc;	/* HUGO Nomeclature Committee ID or 0 if none */
	    km.name = km.hugoSymbol = hm->symbol;	/* HUGO short name */
	    km.hugoName = hm->name;	/* HUGO descriptive name */
	    km.hugoMap = hm->map;	/* HUGO Map position */
	    km.pmId1 = hm->pmId1;	/* I have no idea - grabbed from a HUGO nomeids.txt */
	    km.pmId2 = hm->pmId2;	/* Likewise, I have no idea */
	    km.refSeqAcc = hm->refSeqAcc;	/* Accession of RefSeq mRNA */
	    km.aliases = hm->aliases;	/* Aliases if any.  Comma and space separated list */
	    km.locusLinkId = hm->locusLinkId;	/* Locus link ID */
	    km.gdbId = hm->gdbId;	/* NCBI GDB database ID */
	    }
	}
    knownMoreTabOut(&km, f);
    }
carefulClose(&f);

printf("Loading database %s\n", database);
sqlUpdate(conn, NOSQLINJ "delete from knownMore");
sqlSafef(query, sizeof query, "load data local infile '%s' into table knownMore", tabName);
sqlUpdate(conn, query);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 5)
    usage();
hgKnownMore(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
