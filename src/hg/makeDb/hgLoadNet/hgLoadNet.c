/* hgLoadNet - Load a chain/net file into database. */
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
    int start,end;	    /* Range covered in chromosome. */
    struct align *alignList;  /* List of gap fillers. */
    };

struct align
/* alignments that fill gaps. */
    {
    struct align *next;	   /* Next alignment in list. */
    int tStart,tEnd;	   /* Range covered, always plus strand. */
    struct gap *gapList;   /* List of internal gaps. */
    double score;
    char *qName;
    int qSize;
    char *qStrand;
    int qStart, qEnd;
    int tSize;
    int chainId;
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
  "hgLoadNet - Load a generic net file into database\n"
  "usage:\n"
  "   hgLoadNet database track files(s).net\n"
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
void writeGap(struct gap *g, char *tName, int level, FILE *f)
{
    fprintf(f,"%u\t%s\t%d\t%d\t%d\n",hFindBin(g->start, g->end),tName, level, g->start, g->end);
}
void writeAlignment(struct align *a, char *tName, int level, FILE *f)
{
    fprintf(f,"%u\t%s\t%d\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%d\t%d\n",hFindBin(a->tStart, a->tEnd), tName, level, a->tStart, a->tEnd, a->tSize, a->qName, a->qStrand, a->qStart, a->qEnd, a->qSize, a->chainId);
}
void chromRead(struct lineFile *lf, FILE *gf, FILE *af, struct chrom **cList)
/* Read next chromosome from net file.  Return NULL at EOF. 
   */
{
struct chrom *chrom;
int start,end, wordCount;
char *row[14];
char *line;
int lineSize;
struct gap *g;
struct align *a;
int level = 1;

wordCount = lineFileChopNext(lf, row, 3);
if (wordCount < 3)
    errAbort("Expecting 3 words line %d of %s", lf->lineIx, lf->fileName);
if (!sameString(row[0], "net"))
    errAbort("Expecting 'net' line %d of %s", lf->lineIx, lf->fileName);
AllocVar(chrom);
chrom->name = cloneString(row[1]);
chrom->size = lineFileNeedNum(lf, row, 2);

if (chrom->size <= 0)
    errAbort("Chomosome %s size must be a positive number line %d of %s", row[1], lf->lineIx, lf->fileName);

/* Now read in gap list. */
while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopByWhite(line, row, 11);
    if (wordCount < 3)
        errAbort("Expecting at least 3 words line %d of %s\n", 
        lf->lineIx, lf->fileName);

    if (sameString("gap", row[0]))
        {
        if (wordCount < 4)
            errAbort("Expecting 4 words line %d of %s\n", 
            lf->lineIx, lf->fileName);
        AllocVar(g);
        slAddHead(&chrom->root, g);
        start = lineFileNeedNum(lf, row, 1);
        end = lineFileNeedNum(lf, row, 2);
        g->start = start;
        g->end = end;
        if (start < 0)
            errAbort("start mismatch %d vs %d line %d of %s\n", 
                start, 0, lf->lineIx, lf->fileName);
        if (end > chrom->size)
            errAbort("t end mismatch %d vs %d line %d of %s\n", 
                end, chrom->size, lf->lineIx, lf->fileName);
        writeGap(g,chrom->name, findLevel(line,row[0]),gf);
        }
    else if (sameString("fill", row[0]))
        {
        int newLevel;
        if (wordCount < 11)
            errAbort("Expecting at least 11 words got %d line %d of %s\n", wordCount ,
            lf->lineIx, lf->fileName);
        newLevel = findLevel(line, row[0]);
        AllocVar(a);
        a->tStart = lineFileNeedNum(lf, row, 1);
        a->tEnd = lineFileNeedNum(lf, row, 2);
        a->tSize = lineFileNeedNum(lf, row, 3);
        a->qName = cloneString(row[4]);
        a->qStrand = cloneString(row[5]);
        a->qStart = lineFileNeedNum(lf, row, 6);
        a->qEnd = lineFileNeedNum(lf, row, 7);
        a->qSize = lineFileNeedNum(lf, row, 8);
        a->chainId = lineFileNeedNum(lf, row, 10);

        writeAlignment(a,chrom->name,newLevel,af);
        }
    else if (sameString("net" , row[0]))
        {
        AllocVar(chrom);
        chrom->name = cloneString(row[1]);
        chrom->size = lineFileNeedNum(lf, row, 2);
        slAddHead(cList, chrom);
        }
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
    dyStringAppend(dy, "  chrom varchar(255) not null,\n");
    dyStringAppend(dy, "  level int unsigned not null,\n");
    dyStringAppend(dy, "  chromStart int unsigned not null,\n");
    dyStringAppend(dy, "  chromEnd int unsigned not null,\n");
    dyStringAppend(dy, "#Indices\n");
    if (!noBin)
       dyStringAppend(dy, "  INDEX(chrom(8),bin),\n");
    dyStringAppend(dy, "  INDEX(chrom(8),chromStart)\n");
    dyStringAppend(dy, ")\n");
    sqlRemakeTable(conn, track, dy->string);
    }

dyStringClear(dy);
dyStringPrintf(dy, "load data local infile '%s' into table %s", tab, track);
printf("Loading %s into %s\n", database, track );
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
}

void loadDatabaseAlign(char *database, char *tab, char *track)
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
    dyStringAppend(dy, "  type varchar(255) not null,\n");
    dyStringAppend(dy, "  tName varchar(255) not null,\n");
    dyStringAppend(dy, "  level int unsigned not null,\n");
    dyStringAppend(dy, "  tStart int unsigned not null,\n");
    dyStringAppend(dy, "  tEnd int unsigned not null,\n");
       dyStringAppend(dy, "  qName varchar(255) not null,\n");
       dyStringAppend(dy, "  strand char(1) not null,\n");
       dyStringAppend(dy, "  qStart int unsigned not null,\n");
       dyStringAppend(dy, "  qEnd int unsigned not null,\n");
       dyStringAppend(dy, "  score int unsigned not null,\n");
       dyStringAppend(dy, "  chainId int unsigned not null,\n");
       dyStringAppend(dy, "  qOver int unsigned not null, \n");
       dyStringAppend(dy, "  qFar int unsigned not null, \n");
       dyStringAppend(dy, "  qDup int unsigned not null, \n");
       dyStringAppend(dy, "  tN int unsigned not null, \n");
       dyStringAppend(dy, "  qN int unsigned not null, \n");
       dyStringAppend(dy, "  tR int unsigned not null, \n");
       dyStringAppend(dy, "  qR int unsigned not null, \n");
       dyStringAppend(dy, "  tNewR int unsigned not null, \n");
       dyStringAppend(dy, "  qNewR int unsigned not null, \n");
       dyStringAppend(dy, "  tOldR int unsigned not null, \n");
       dyStringAppend(dy, "  qOldR int unsigned not null, \n");
       dyStringAppend(dy, "  tTrf int unsigned not null, \n");
       dyStringAppend(dy, "  qTrf int unsigned not null, \n");
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

void cnWriteTables(char *chrom, struct cnFill *fillList, FILE *f, int depth)
/* Recursively write out fill and gap lists. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    char *type = (fill->chainId ? "fill" : "gap");
    if (!fill->score)
        fill->score = 0;
    if (fill->score < 0)
        fill->score = 0;
    fprintf(f, "%d\t%s\t%s\t%d\t%d\t%d\t%s\t%c\t%d\t%d", hFindBin(fill->tStart, (fill->tStart)+(fill->tSize)),type, chrom, fill->chainId ? (depth+1)/2 : depth/2, fill->tStart, (fill->tStart)+(fill->tSize),
    	fill->qName, fill->qStrand, fill->qStart, fill->qStart+fill->qSize);
    if (fill->chainId)
        fprintf(f, "\t%d", fill->chainId);
    fprintf(f, "\t%1.0f", fill->score);
    if (fill->qOver < 0)
        fill->qOver = 0;
        fprintf(f, "\t%d", fill->qOver);
    if (fill->qFar <= 0)
        fill->qFar = 0;
        fprintf(f, "\t%d", fill->qFar);
    if (fill->qDup <= 0)
        fill->qDup = 0;
        fprintf(f, "\t%d", fill->qDup);
    if (fill->tN <= 0)
        fill->tN = 0;
        fprintf(f, "\t%d", fill->tN);
    if (fill->qN <= 0)
        fill->qN = 0;
        fprintf(f, "\t%d", fill->qN);
    if (fill->tR <= 0)
        fill->tR = 0;
        fprintf(f, "\t%d", fill->tR);
    if (fill->qR <= 0)
        fill->qR = 0;
        fprintf(f, "\t%d", fill->qR);
    if (fill->tNewR <= 0)
        fill->tNewR = 0;
        fprintf(f, "\t%d", fill->tNewR);
    if (fill->qNewR <= 0)
        fill->qNewR = 0;
        fprintf(f, "\t%d", fill->qNewR);
    if (fill->tOldR <= 0)
        fill->tOldR = 0;
        fprintf(f, "\t%d", fill->tOldR);
    if (fill->qOldR <= 0)
        fill->qOldR = 0;
        fprintf(f, "\t%d", fill->qOldR);
    if (fill->tTrf < 0)
        fill->tTrf = 0;
        fprintf(f, "\t%d", fill->tTrf);
    if (fill->qTrf < 0)
        fill->qTrf = 0;
        fprintf(f, "\t%d", fill->qTrf);
    fputc('\n', f);
    if (fill->children)
        cnWriteTables(chrom, fill->children, f, depth+1);
    }
}
void hgLoadNet(char *database, char *track, int netCount, char *netFiles[])
/* hgLoadNet - Load a net file into database. */
{
int i;
struct hash *qHash, *tHash;
struct chrom *chromList = NULL, *chrom;
struct chainNet *cn;
struct lineFile *lf ;
struct chainNet *net;
char alignFileName[] ="align.tab";
FILE *alignFile = mustOpen(alignFileName,"w");

for (i=0; i<netCount; ++i)
    {
    lf = lineFileOpen(netFiles[i], TRUE);
    printf("file is %s\n",netFiles[i]);
    /*chromRead(lf, gapFile, alignFile, &chromList);*/
    while ((net = chainNetRead(lf)) != NULL)
        {
        printf("read %s\n",net->name);
        cnWriteTables(net->name,net->fillList, alignFile, 1);
        }

    }
fclose(alignFile);
/*slSort(&bedList, bedStubCmp);
printf("Sorted\n");*/
loadDatabaseAlign(database, alignFileName, track);
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
hgLoadNet(argv[1], argv[2], argc-3, argv+3);
return 0;
}
