/* estLibStats - Calculate some stats on EST libraries given file from polyInfo. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "estInfo.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "estLibStats - Calculate some stats on EST libraries given file from polyInfo\n"
  "usage:\n"
  "   estLibStats database eiInfo.bed output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

int scoreEi(struct estInfo *ei, int *retStrand)
/* Assign a score to an ei.  retStrand if non-null
 * returns +1 or -1 depending which strand est appears
 * to be on relative to transcript. */
{
int fScore = 0, rScore = 0, score, strand;

if (ei->intronOrientation > 0)
    fScore += ei->intronOrientation * 15;
else if (ei->intronOrientation < 0)
    rScore += -ei->intronOrientation * 15;
fScore += ei->sizePolyA;
rScore += ei->sizePolyA;
if (ei->signalPos > 0)
    fScore += 8;
if (ei->revSignalPos > 0)
    rScore += 8;
if (fScore >= rScore)
    {
    score = fScore;
    strand = 1;
    }
else
    {
    score = rScore;
    strand = -1;
    }
if (score < 6)
    strand = 0;
if (retStrand != NULL)
     *retStrand = strand;
return score;
}

struct hash *readEstInfo(char *fileName)
/* Read in EST info from file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[9];
struct hash *hash = newHash(20);

while (lineFileRow(lf, row))
    {
    struct estInfo *ei = estInfoLoad(row), *oldEi;
    oldEi = hashFindVal(hash, ei->name);
    if (oldEi != NULL)
        {
	if (scoreEi(ei, NULL) < scoreEi(oldEi, NULL))
	     {
	     estInfoFree(&ei);
	     continue;
	     }
	}
    hashAdd(hash, ei->name, ei);
    }
return hash;
}


struct libInfo
/* Info on a library */
    {
    struct libInfo *next;
    char *id;			/* Id in ascii form. */
    char *name;			/* Full name of library. */
    int estCount;		/* Total count of ESTs. */
    struct estInfo *threePrime;	/* List of 3' ESTs. */
    struct estInfo *fivePrime;      /* List of 5' ESTs. */
    struct estInfo *unPrime;       /* List of unknown orientation ests. */
    };

void readLibs(char *database, struct libInfo **retList, struct hash **retHash)
/* Scan database for libraries and put in list. */
{
struct sqlConnection *conn = sqlConnect(database);
struct hash *hash = newHash(0);
struct sqlResult *sr;
char **row;
struct libInfo *liList = NULL, *li;

sr = sqlGetResult(conn, "select id,name from author");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(li);
    li->name = cloneString(row[1]);
    hashAddSaveName(hash, row[0], li, &li->id);
    slAddHead(&liList, li);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&liList);
*retList = liList;
*retHash = hash;
}

void addEsts(char *database, struct hash *eiHash, struct hash *liHash)
/* Read in all ESTs from mRNA table in database and add them to 
 * library they belong to. */
{
struct libInfo *li;
struct estInfo *ei;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr = NULL;
char **row;

sr = sqlGetResult(conn, "select acc,author,direction from mrna where type = 'EST'");
while ((row = sqlNextRow(sr)) != NULL)
    {
    li = hashMustFindVal(liHash, row[1]);
    li->estCount += 1;
    if ((ei = hashFindVal(eiHash, row[0])) != NULL)
        {
	switch (row[2][0])
	    {
	    case '5':
	        slAddHead(&li->fivePrime, ei);
		break;
	    case '3':
	        slAddHead(&li->threePrime, ei);
		break;
	    case '0':
	        slAddHead(&li->unPrime, ei);
		break;
	    default:
	        errAbort("Unknown type '%s' for %s", row[2], row[0]);
		break;
	    }
	}
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

void sumStrandAndScore(struct estInfo *eiList, int *retStrand, int *retScore)
/* Return sum of strand and score. */
{
struct estInfo *ei;
int strand = 0, score = 0, strand1, score1;
for (ei = eiList; ei != NULL; ei = ei->next)
    {
    score1 = scoreEi(ei, &strand1);
    score += score1;
    strand += strand1;
    }
*retScore = score;
*retStrand = strand;
}

void labelGroup(FILE *f, char *name)
/* Print label of a couple elements. */
{
fprintf(f, "%s\torient\tsplice+\tsplice-\tsignal+\tsignal-\tpolyA+\tpolyA-\t", name);
}

void printGroupStats(FILE *f, struct estInfo *eiList)
/* Print stats from list. */
{
int splicePlus=0, spliceMinus=0, signalPlus=0, signalMinus=0, polyPlus=0, polyMinus=0;
int all = 0, orient = 0, strand;
struct estInfo *ei;

for (ei=eiList; ei != NULL; ei = ei->next)
    {
    ++all;
    scoreEi(ei, &strand);
    if (strand != 0)
        ++orient;
    if (ei->intronOrientation > 0)
        ++splicePlus;
    else if (ei->intronOrientation < 0)
        ++spliceMinus;
    if (ei->signalPos > 0)
        ++signalPlus;
    if (ei->revSignalPos > 0)
        ++signalMinus;
    if (ei->sizePolyA >= 6)
        ++polyPlus;
    if (ei->revSizePolyA >= 6)
        ++polyMinus;
    }
fprintf(f, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t",
    all, orient,
    splicePlus, spliceMinus,
    signalPlus, signalMinus,
    polyPlus, polyMinus);
}


void addSizeAndDates(struct estInfo *eiList, long *size, int *count, char **minDate, char **maxDate)
/* Fold in size and date info from this est list. */
{
struct estInfo *ei;
for (ei = eiList; ei != NULL; ei = ei->next)
    {
    *size += ei->size;
    if (*minDate == NULL || strcmp(ei->date, *minDate) < 0)
         *minDate = ei->date;
    if (*maxDate == NULL || strcmp(ei->date, *maxDate) > 0)
         *maxDate = ei->date;
    *count += 1;
    }
}

void printLibStats(struct libInfo *liList, char *fileName)
/* Write stats on libs to file. */
{
struct libInfo *li;
FILE *f = mustOpen(fileName, "w");
int score, strand;

fprintf(f, "#ests\tsize\tstartDate\tendngDate\t");
labelGroup(f, "3'");
labelGroup(f, "5'");
labelGroup(f, "?");
fprintf(f, "name\n");

for (li = liList; li != NULL; li = li->next)
    {
    char *minDate = NULL, *maxDate = NULL;
    long totalSize = 0;
    int count = 0, avgSize = 0;

    if (li->estCount == 0)
        continue;
    fprintf(f, "%d\t", li->estCount);
    addSizeAndDates(li->threePrime, &totalSize, &count, &minDate, &maxDate);
    addSizeAndDates(li->fivePrime, &totalSize, &count, &minDate, &maxDate);
    addSizeAndDates(li->unPrime, &totalSize, &count, &minDate, &maxDate);
    if (count > 0)
        avgSize = totalSize/count;
    fprintf(f, "%d\t%s\t%s\t", avgSize, minDate, maxDate);
    printGroupStats(f, li->threePrime);
    printGroupStats(f, li->fivePrime);
    printGroupStats(f, li->unPrime);
    fprintf(f, "%s\n", li->name);
    }
carefulClose(&f);
}

int libInfoCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct libInfo *a = *((struct libInfo **)va);
const struct libInfo *b = *((struct libInfo **)vb);
return b->estCount - a->estCount;
}

void addSizes(char *database, struct hash *eiHash, struct hash *dateHash)
/* Add and date info to ests in eiHash. Date strings are stored in dateHash */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr = NULL;
char **row;

sr = sqlGetResult(conn, "select acc,size,gb_date from seq");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *acc = row[0];
    struct estInfo *ei = hashFindVal(eiHash, acc);
    if (ei != NULL)
        {
	ei->size = sqlUnsigned(row[1]);
	ei->date = hashStoreName(dateHash, row[2]);
	}
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

void estLibStats(char *database, char *eiInfoBed, char *output)
/* estLibStats - Calculate some stats on EST libraries given file from polyInfo. */
{
struct hash *eiHash = NULL, *liHash = NULL;
struct hash *dateHash = newHash(0);
struct libInfo *liList = NULL, *li;

readLibs(database, &liList, &liHash);
printf("Read %d libs\n", slCount(liList));
eiHash = readEstInfo(eiInfoBed);
printf("Read %s\n", eiInfoBed);
addSizes(database, eiHash, dateHash);
printf("Added size info\n");
addEsts(database, eiHash, liHash);
printf("Read in 3' and other info from %s\n", database);
slSort(&liList, libInfoCmp);
printLibStats(liList, output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
estLibStats(argv[1], argv[2], argv[3]);
return 0;
}
