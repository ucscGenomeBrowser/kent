/* hgLoadChain - Load a chain file into database. */
#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "dystring.h"
#include "bed.h"
#include "hdb.h"
#include "chainNet.h"

/* Command line switches. */
boolean noBin = FALSE;		/* Suppress bin field. */
boolean strictTab = FALSE;	/* Separate on tabs. */
boolean oldTable = FALSE;	/* Don't redo table. */
char *sqlTable = NULL;		/* Read table from this .sql if non-NULL. */


struct gap
/* A gap in sequence alignments. */
    {
    struct gap *next;	    /* Next gap in list. */
    int tStart,tEnd, tSize;	    /* Range covered in chromosome. */
    int qStart;	    /* gap size in mouse */
    int chainId;            /* pointer to parent chain id*/
    };

struct chain
/* chain structure */
    {
    struct chain *next;	   /* Next chain in list. */
    double score;
    char *tName;
    int tSize;              /* size of chromosome */
    char *tStrand;
    int tStart,tEnd;	   /* Range covered, always plus strand. */
    char *qName;
    int qSize;
    char *qStrand;
    int qStart, qEnd;
    int id;            /* chain id */
    struct gap *gapList;   /* List of internal gaps. */
    };

struct space
/* A part of a gap. */
    {
    struct gap *gap;    /* The actual gap. */
    int start, end;	/* Portion of gap covered, always plus strand. */
    };

struct chrom
/* A chromosome. */
    {
    struct chrom *next;	    /* Next in list. */
    char *name;		    /* Name - allocated in hash */
    int size;		    /* Size of chromosome. */
    struct gap *root;	    /* Root of the gap/chain tree */
    struct rbTree *spaces;  /* Store gaps here for fast lookup. */
    };


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadChain - Load a generic Chain file into database\n"
  "usage:\n"
  "   hgLoadChain database track files(s).Chain\n"
  "options:\n"
  "   -noBin   suppress bin field\n"
  "   -oldTable add to existing table\n"
  "   -sqlTable=table.sql Create table from .sql file\n"
  "   -tab  Separate by tabs rather than space\n"
  );
}

int findLevel(char *line, char *type)
    /* return level of alignment based on number of spaces before the record */
{
    int i;
    char *lp = line;
    for (i=0; line[i] != 0; i++)
        {
        if (*lp == type[0])
            break;
        lp++;
        }
    if (sameString(type, "fill"))
        {
        return (i+1)/2;
        }
    if (sameString(type, "gap"))
        {
        return (i-1)/2;
        }
}
void writeGap(struct gap *g, char *tName, FILE *f)
{
    fprintf(f,"%u\t%s\t%d\t%d\t%d\t%d\n",hFindBin(g->tStart, g->tEnd), tName, g->tStart, (g->tEnd), g->qStart, g->chainId);
}
void writeChain(struct chain *a, FILE *f)
{
    fprintf(f,"%u\t%10.0f\t%s\t%d\t%d\t%d\t%s\t%d\t%s\t%d\t%d\t%d\n",hFindBin(a->tStart, a->tEnd), a->score, a->tName, a->tSize, a->tStart, a->tEnd, a->qName, a->qSize, a->qStrand, a->qStart, a->qEnd, a->id);
    /*fprintf(f,"%u\t%10.0f\t%s\t%d\t%s\t%d\t%d\t%s\t%d\t%d\t%d\t%d\n",hFindBin(a->tStart, a->tEnd), a->score, a->qName, a->qSize, a->qStrand,a->qStart, a->qEnd, a->tName, a->tSize,  a->tStart, a->tEnd, a->id);*/
}
void chainRead(struct lineFile *lf, FILE *gf, FILE *af, struct chrom **cList)
/* Read next chromosome from Chain file.  Return NULL at EOF. 
   */
{
struct chrom *chrom;
int size, tGap, qGap, start,end, wordCount;
char *row[14];
char *line;
int lineSize;
struct gap *g;
struct chain *c;
int prevTstart = 0, prevQstart = 0, level = 1;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopByWhite(line, row, 13);
    if (sameString("chain", row[0]))
        {
        int newLevel;

        /* finish last block if not first chain */
        if (prevTstart != 0 && prevQstart !=0)
            {
            g->tStart = prevTstart;
            g->tEnd = g->tStart + size;
            g->qStart = prevQstart;
            g->tSize = size;
            g->chainId = c->id;
            if (g->tStart < 0)
                errAbort("start mismatch %d vs %d line %d of %s\n", 
                    g->tStart, 0, lf->lineIx, lf->fileName);
            if (g->tEnd > c->tSize)
                errAbort("tEend mismatch %d vs %d line %d of %s\n", 
                    g->tEnd, c->tSize, lf->lineIx, lf->fileName);
            writeGap(g,c->tName, gf);
            }
        if (wordCount < 13)
            errAbort("Expecting at least 13 words got %d line %d of %s\n", wordCount ,
            lf->lineIx, lf->fileName);
        if (!sameString(row[0], "chain"))
            errAbort("Expecting 'chain' line %d of %s", lf->lineIx, lf->fileName);
        AllocVar(c);
        c->score = lineFileNeedNum(lf, row, 1);
        c->tName = cloneString(row[2]);
        c->tSize = lineFileNeedNum(lf, row, 3);
        c->tStrand = cloneString(row[4]);
        c->tStart = lineFileNeedNum(lf, row, 5);
        c->tEnd = lineFileNeedNum(lf, row, 6);
        c->qName = cloneString(row[7]);
        c->qSize = lineFileNeedNum(lf, row, 8);
        c->qStrand = cloneString(row[9]);
        c->qStart = lineFileNeedNum(lf, row, 10);
        c->qEnd = lineFileNeedNum(lf, row, 11);
        c->id = lineFileNeedNum(lf, row, 12);
        prevTstart = c->tStart;
        prevQstart = c->qStart;

        writeChain(c,af);
        }
    else  
        {
        if (wordCount < 3)
            continue;
        /*
            errAbort("Expecting 3 words line %d of %s\n", 
            lf->lineIx, lf->fileName);
        */
        AllocVar(g);
/*        slAddHead(&chrom->root, g);*/
        size = lineFileNeedNum(lf, row, 0);
        tGap = lineFileNeedNum(lf, row, 1);
        qGap = lineFileNeedNum(lf, row, 2);
        g->tStart = prevTstart;
        g->tEnd = g->tStart + size;
        g->qStart = prevQstart;
        g->tSize = size;
        g->chainId = c->id;
        if (g->tStart < 0)
            errAbort("start mismatch %d vs %d line %d of %s\n", 
                g->tStart, 0, lf->lineIx, lf->fileName);
        if (g->tEnd > c->tSize)
            errAbort("tEend mismatch %d vs %d line %d of %s\n", 
                g->tEnd, c->tSize, lf->lineIx, lf->fileName);
        writeGap(g,c->tName, gf);
        prevTstart = g->tEnd + tGap;
        prevQstart = g->qStart + size + qGap;
        }
    /*
    else if (sameString("chainx" , row[0]))
        {
        AllocVar(chrom);
        chrom->name = cloneString(row[1]);
        >size = lineFileNeedNum(lf, row, 2);
        slAddHead(cList, chrom);
        }
        */
    }
return ;
}
int findBedSize(char *fileName)
/* Read first line of file and figure out how many words in it. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line;
int wordCount;
lineFileNeedNext(lf, &line, NULL);
if (strictTab)
    wordCount = chopTabs(line, words);
else
    wordCount = chopLine(line, words);
if (wordCount == 0)
    errAbort("%s appears to be empty", fileName);
lineFileClose(&lf);
return wordCount;
}

struct bedStub
/* A line in a bed file with chromosome, start, end position parsed out. */
    {
    struct bedStub *next;	/* Next in list. */
    char *chrom;                /* Chromosome . */
    int chromStart;             /* Start position. */
    int chromEnd;		/* End position. */
    char *line;                 /* Line. */
    };

int bedStubCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct bedStub *a = *((struct bedStub **)va);
const struct bedStub *b = *((struct bedStub **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}


void loadOneBed(char *fileName, int bedSize, struct bedStub **pList)
/* Load one bed file.  Make sure all lines have bedSize fields.
 * Put results in *pList. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line, *dupe;
int wordCount;
struct bedStub *bed;
boolean tab = 

printf("Reading %s\n", fileName);
while (lineFileNext(lf, &line, NULL))
    {
    dupe = cloneString(line);
    if (strictTab)
	wordCount = chopTabs(line, words);
    else
	wordCount = chopLine(line, words);
    lineFileExpectWords(lf, bedSize, wordCount);
    AllocVar(bed);
    bed->chrom = cloneString(words[0]);
    bed->chromStart = lineFileNeedNum(lf, words, 1);
    bed->chromEnd = lineFileNeedNum(lf, words, 2);
    bed->line = dupe;
    slAddHead(pList, bed);
    }
lineFileClose(&lf);
}

void writeTab(char *fileName, struct bedStub *bedList, int bedSize)
/* Write out bed list to tab-separated file. */
{
struct bedStub *bed;
FILE *f = mustOpen(fileName, "w");
char *words[64];
int i, wordCount;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (!noBin)
        fprintf(f, "%u\t", hFindBin(bed->chromStart, bed->chromEnd));
    if (strictTab)
	wordCount = chopTabs(bed->line, words);
    else
	wordCount = chopLine(bed->line, words);
    for (i=0; i<wordCount; ++i)
        {
	fputs(words[i], f);
	if (i == wordCount-1)
	    fputc('\n', f);
	else
	    fputc('\t', f);
	}
    }
fclose(f);
}

void loadDatabaseGap(char *database, char *tab, char *track)
/* Load database from tab file. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *dy = newDyString(1024);
/* First make table definition. */
if (sqlTable != NULL)
    {
    /* Read from file. */
    char *sql, *s;
    readInGulp(sqlTable, &sql, NULL);

    /* Chop of end-of-statement semicolon if need be. */
    s = strchr(sql, ';');
    if (s != NULL) *s = 0;
    
    sqlRemakeTable(conn, track, sql);
    freez(&sql);
    }
else if (!oldTable)
    {
    /* Create definition statement. */
    printf("Creating table definition for %s\n", track);
    dyStringPrintf(dy, "CREATE TABLE %s (\n", track);
    if (!noBin)
       dyStringAppend(dy, "  bin smallint unsigned not null,\n");
       dyStringAppend(dy, "  tName varchar(255) not null,\n");
    dyStringAppend(dy, "  tStart int unsigned not null,\n");
    dyStringAppend(dy, "  tEnd int unsigned not null,\n");
    dyStringAppend(dy, "  qStart int unsigned not null,\n");
    dyStringAppend(dy, "  chainId int unsigned not null,\n");
    dyStringAppend(dy, "#Indices\n");
    if (!noBin)
       dyStringAppend(dy, "  INDEX(chainId,bin),\n");
    dyStringAppend(dy, "  INDEX(chainId,tStart)\n");
    dyStringAppend(dy, ")\n");
    sqlRemakeTable(conn, track, dy->string);
    }

dyStringClear(dy);
dyStringPrintf(dy, "load data local infile '%s' into table %s", tab, track);
printf("Loading %s into %s\n", database, track );
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
}

void loadDatabaseChain(char *database, char *tab, char *track)
/* Load database from tab file. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *dy = newDyString(1024);
/* First make table definition. */
if (sqlTable != NULL)
    {
    /* Read from file. */
    char *sql, *s;
    readInGulp(sqlTable, &sql, NULL);

    /* Chop of end-of-statement semicolon if need be. */
    s = strchr(sql, ';');
    if (s != NULL) *s = 0;
    
    sqlRemakeTable(conn, track, sql);
    freez(&sql);
    }
else if (!oldTable)
    {
    /* Create definition statement. */
    printf("Creating table definition for %s\n", track);
    dyStringPrintf(dy, "CREATE TABLE %s (\n", track);
    if (!noBin)
       dyStringAppend(dy, "  bin smallint unsigned not null,\n");
       dyStringAppend(dy, "  score double not null,\n");
       dyStringAppend(dy, "  tName varchar(255) not null,\n");
       dyStringAppend(dy, "  tSize int unsigned not null, \n");
       dyStringAppend(dy, "  tStart int unsigned not null,\n");
       dyStringAppend(dy, "  tEnd int unsigned not null,\n");
       dyStringAppend(dy, "  qName varchar(255) not null,\n");
       dyStringAppend(dy, "  qSize int unsigned not null,\n");
       dyStringAppend(dy, "  qStrand char(1) not null,\n");
       dyStringAppend(dy, "  qStart int unsigned not null,\n");
       dyStringAppend(dy, "  qEnd int unsigned not null,\n");
       dyStringAppend(dy, "  id int unsigned not null,\n");
    dyStringAppend(dy, "#Indices\n");
    if (!noBin)
       dyStringAppend(dy, "  INDEX(tName(8),bin),\n");
    dyStringAppend(dy, "  INDEX(tName(8),tStart)\n");
    dyStringAppend(dy, ")\n");
    sqlRemakeTable(conn, track, dy->string);
    }

dyStringClear(dy);
dyStringPrintf(dy, "load data local infile '%s' into table %s", tab, track);
printf("Loading %s into %s\n", database, track );
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
}

void hgLoadChain(char *database, char *track, int chainCount, char *chainFiles[])
/* hgLoadChain - Load a Chain file into database. */
{
int i;
struct hash *qHash, *tHash;
struct chrom *chromList = NULL, *chrom;
struct chainNet *cn;
struct lineFile *lf ;
struct chainNet *chain;
char alignFileName[] ="chain.tab";
char gapFileName[] ="gap.tab";
FILE *alignFile = mustOpen(alignFileName,"w");
FILE *gapFile = mustOpen(gapFileName,"w");
char gapTrack[128];

sprintf(gapTrack, "%sGap",track);
for (i=0; i<chainCount; ++i)
    {
    lf = lineFileOpen(chainFiles[i], TRUE);
    printf("file is %s\n",chainFiles[i]);
    chainRead(lf, gapFile, alignFile, &chromList);

    }
fclose(alignFile);
fclose(gapFile);
/*slSort(&bedList, bedStubCmp);
printf("Sorted\n");*/
loadDatabaseChain(database, alignFileName, track);
loadDatabaseGap(database, gapFileName, gapTrack);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 4)
    usage();
noBin = cgiBoolean("noBin");
strictTab = cgiBoolean("tab");
oldTable = cgiBoolean("oldTable");
sqlTable = cgiOptionalString("sqlTable");
hgLoadChain(argv[1], argv[2], argc-3, argv+3);
return 0;
}
