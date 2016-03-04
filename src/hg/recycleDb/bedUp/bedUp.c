/* bedUp - Load bed submissions after conversion back into new database.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "psl.h"
#include "clonePos.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedUp - Load bed submissions after conversion back into new database.\n"
  "usage:\n"
  "   bedUp oldDb table newDb old.tab convert.psl missing\n");
}

struct bedExt
/* Extended bed structure. */
    {
    struct bedExt *next;     /* Next in singly linked list. */
    char *chrom;	  /* Human chromosome or FPC contig */
    unsigned chromStart;  /* Start position in chromosome */
    unsigned chromEnd;	  /* End position in chromosome */
    char *name;           /* Name. */
    char **extraWords;    /* Other words in this row of database. */
    char *id;             /* Unique ID - allocated in hash. */
    struct psl *pslList;  /* Conversion psl. */
    char *oldClone;       /* Old clone this used to be at. */
    bool converted;       /* TRUE if converted to new coors. */
    };

char *bedIdS(char *name, char *chrom, char *chromStart)
/* Return unique identifier for bed treating number as string. */
{
static char buf[128];
sprintf(buf, "%s.%s.%s", name, chrom, chromStart);
return buf;
}

char *bedId(char *name, char *chrom, int chromStart)
/* Return unique identifier for bed. */
{
char num[16];
sprintf(num, "%d", chromStart);
return bedIdS(name, chrom, num);
}

void unpackPslName(struct lineFile *lf, char *pslName, char **retName, char **retDb, char **retChrom, int *retStart)
/* Unpack name in convert psl file. */
{
static char buf[128];
char *words[8];
int i;
char *e;

strncpy(buf, pslName, sizeof(buf));
for (i=0; i<3; ++i)
    {
    e = strrchr(buf, '.');
    if (e == NULL)
        errAbort("Expecting name.db.chrom.start line %d of %s", lf->lineIx, lf->fileName);
    *e++ = 0;
    words[i] = e;
    }
*retDb = words[2];
*retChrom = words[1];
*retStart = atoi(words[0]);
*retName = buf;
}

struct bedExt *loadTable(char *fileName, struct hash *bedHash, struct hash *chromHash, struct hash *cloneHash)
/* Make list and load up tables from file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct bedExt *bedList = NULL, *bed;
char *words[16];
int wordCount;
int width;
char *s;
struct hashEl *hel;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, 10, wordCount);
    AllocVar(bed);
    bed->chrom = hashStoreName(chromHash, words[2]);
    bed->chromStart = bed->chromEnd = lineFileNeedNum(lf, words, 3);
    width = lineFileNeedNum(lf, words, 4);
    bed->chromEnd += width;
    s = words[5];
    chopSuffix(s);
    bed->oldClone = hashStoreName(cloneHash, s);
    bed->name = cloneString(words[0]);
    s = bedId(bed->name, bed->chrom, bed->chromStart);
    if (hashLookup(bedHash, s))
       {
       warn("%s duplicated, ignoring all but first.", s);
       freez(&bed);
       }
    else
       {
	hel = hashAddUnique(bedHash, s, bed);
	bed->id = hel->name;
	slAddHead(&bedList, bed);
       }
    }
lineFileClose(&lf);
slReverse(&bedList);
return bedList;
}

void addPsls(char *fileName, char *oldDb, struct hash *bedHash)
/* Add relevant psls to beds. */
{
struct bedExt *bed;
struct lineFile *lf = pslFileOpen(fileName);
struct psl *psl;
char *s;
char *name, *db, *chrom;
int chromStart;
int total = 0, uniq = 0;

while ((psl = pslNext(lf)) != NULL)
    {
    unpackPslName(lf, psl->qName, &name, &db, &chrom, &chromStart);
    if (!sameString(db, oldDb))
        errAbort("Old database mismatch %s in command line %s in %s", oldDb, db, fileName);
    s = bedId(name, chrom, chromStart);
    bed = hashMustFindVal(bedHash, s);
    if (bed->pslList == NULL)
        uniq += 1;
    slAddHead(&bed->pslList, psl);
    ++total;
    }
lineFileClose(&lf);
printf("%d psls int %s added to %d beds\n", total, fileName, uniq);
}

void fixFishChrom(char *s)
/* Fix chrna_random to chrNA_random, etc for fish. */
{
char *e;
s += 3;
if (islower(s[0]))
    {
    e = strchr(s, '_');
    if (e == NULL)
       e = s + strlen(s);
    toUpperN(s, e-s);
    }
}

void addOther(char *oldDb, char *oldTable, struct hash *bedHash)
/* Add extra rows to beds. */
{
struct sqlConnection *conn = sqlConnect(oldDb);
struct sqlResult *sr;
char **row;
char query[256];
int columnCount;
int extraCount, i;
char *s;
struct bedExt *bed;
enum {definedCol = 4};
int rowCount = 0;


sqlSafef(query, sizeof query, "select * from %s", oldTable);
sr = sqlGetResult(conn, query);
columnCount = sqlCountColumns(sr);
extraCount = columnCount - definedCol;
while ((row = sqlNextRow(sr)) != NULL)
    {
    fixFishChrom(row[0]);
    s = bedIdS(row[3], row[0], row[1]);
    bed = hashMustFindVal(bedHash, s);
    if (bed->extraWords != NULL)
	{
        warn("Duplicate %s in database, ignoring all but first.", s);
	continue;
	}
    AllocArray(bed->extraWords, extraCount);
    for (i=definedCol; i<columnCount; ++i)
        bed->extraWords[i-definedCol] = cloneString(row[i]);
    ++rowCount;
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
printf("Added %d extra columns to %d rows\n", extraCount, rowCount);
}

struct clonePos *loadClonePos(char *db, struct hash *clonePosHash)
/* Load up clonePos list/hash from database table. */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row;
char name[128];
struct clonePos *el, *list = NULL;

sr = sqlGetResult(conn, NOSQLINJ "select * from clonePos");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = clonePosLoad(row);
    strcpy(name, el->name);
    chopSuffix(name);
    hashAdd(clonePosHash, name, el);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&list);
printf("Loaded %d clonePos from %s\n", slCount(list), db);
return list;
}


void convertOne(struct bedExt *bed, struct hash *clonePosHash)
/* Convert a single bed if possible. */
{
int bestScore = 0, score;
struct psl *psl, *bestPsl = NULL;
int milliBad;
struct clonePos *clonePos;

/* Find best psl in list. */
for (psl = bed->pslList; psl != NULL; psl = psl->next)
    {
    score = psl->match + psl->repMatch;
    if (score < round(0.80*psl->qSize))
        continue;
    if ((milliBad = pslCalcMilliBad(psl, FALSE)) > 30)
        continue;
    score = round( 0.02*(50-milliBad)*score);
    clonePos = hashFindVal(clonePosHash, bed->oldClone);
    if (clonePos != NULL)
	{
	if (sameString(clonePos->chrom, psl->tName))
	    {
	    if (rangeIntersection(psl->tStart, psl->tEnd, clonePos->chromStart, clonePos->chromEnd) > 0)
		score += 100;
	    }
	}
    if (score > bestScore)
        {
	bestScore = score;
	bestPsl = psl;
	}
    }
if (bestPsl)
    {
    bed->converted = TRUE;
    }
}

void convertList(struct bedExt *bedList, struct hash *clonePosHash)
/* Convert list to new coordinates if possible. */
{
struct bedExt *bed;
int convertCount = 0;
int totalCount = 0;
for (bed = bedList; bed != NULL; bed = bed->next)
    if (bed->extraWords && bed->pslList)
	{
	convertOne(bed, clonePosHash);
	++totalCount;
	if (bed->converted)
	    ++convertCount;
	}
printf("%d of %d converted\n", convertCount, totalCount);
}

void bedUp(char *oldDb, char *oldTable, char *newDb, char *tableFile, char *pslFile, 
	char *missing)
/* bedUp - Load bed submissions after conversion back into new database.. */
{
struct bedExt *bedList = NULL;
struct hash *bedHash = newHash(0);
struct hash *chromHash = newHash(5);
struct hash *cloneHash = newHash(0);
struct hash *clonePosHash = newHash(0);
// struct clonePos *clonePosList = NULL;

bedList = loadTable(tableFile, bedHash, chromHash, cloneHash);
printf("Loaded %d items from %s\n", slCount(bedList), tableFile);
addPsls(pslFile, oldDb, bedHash);
addOther(oldDb, oldTable, bedHash);
// clonePosList = loadClonePos(newDb, clonePosHash);
//   answer is returned in clonePosHash
(void) loadClonePos(newDb, clonePosHash);  // ignore returned has pointer
convertList(bedList, clonePosHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 7)
    usage();
bedUp(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
