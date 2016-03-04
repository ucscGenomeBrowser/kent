/* estLibStats - Calculate some stats on EST libraries given file from polyInfo. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "estOrientInfo.h"


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

int scoreEi(struct estOrientInfo *ei, int *retStrand)
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
    struct estOrientInfo *ei = estOrientInfoLoad(row), *oldEi;
    oldEi = hashFindVal(hash, ei->name);
    if (oldEi != NULL)
        {
	if (scoreEi(ei, NULL) < scoreEi(oldEi, NULL))
	     {
	     estOrientInfoFree(&ei);
	     continue;
	     }
	}
    hashAdd(hash, ei->name, ei);
    }
return hash;
}


struct libInfo
/* Info on a batch of ESTs */
    {
    struct libInfo *next;
    char *libName;		/* Full name of library. */
    char *author;		/* Library authors. */
    int estCount;		/* Total count of ESTs. */
    struct estOrientInfo *threePrime;	/* List of 3' ESTs. */
    struct estOrientInfo *fivePrime;      /* List of 5' ESTs. */
    struct estOrientInfo *unPrime;       /* List of unknown orientation ests. */
    };

int libInfoCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct libInfo *a = *((struct libInfo **)va);
const struct libInfo *b = *((struct libInfo **)vb);
return b->estCount - a->estCount;
}

struct nameId
/* A name/id pair. */
    {
    struct nameId *next;
    char *id;		/* Ascii numerical id. */
    char *name;		/* Often longish name with white space. */
    };

struct hash *readIdHash(char *database, char *table)
/* Put an id/name table into a hash keyed by id with
 * nameId as value. */
{
struct sqlConnection *conn = sqlConnect(database);
struct hash *hash = newHash(0);
char query[256];
struct sqlResult *sr;
char **row;
struct nameId *nid;

sqlSafef(query, sizeof query, "select id,name from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(nid);
    nid->name = cloneString(row[1]);
    hashAddSaveName(hash, row[0], nid, &nid->id);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return hash;
}

struct libInfo *addEsts(char *database, struct hash *eiHash, 
	struct hash *libHash, struct hash *authorHash,
	struct hash *liHash)
/* Read in all ESTs from mRNA table in database and add them to 
 * library they belong to. */
{
struct libInfo *liList = NULL, *li;
struct estOrientInfo *ei;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr = NULL;
char liId[256];
char **row;
struct nameId *library, *author;

sr = sqlGetResult(conn, NOSQLINJ "select acc,library,author,direction from mrna where type = 'EST'");
while ((row = sqlNextRow(sr)) != NULL)
    {
    library = hashMustFindVal(libHash, row[1]);
    author = hashMustFindVal(authorHash, row[2]);
    sprintf(liId, "%s.%s", library->id, author->id);
    if ((li = hashFindVal(liHash, liId)) == NULL)
        {
	AllocVar(li);
	li->libName = library->name;
	li->author = author->name;
	hashAdd(liHash, liId, li);
	slAddHead(&liList, li);
	}
    li->estCount += 1;
    if ((ei = hashFindVal(eiHash, row[0])) != NULL)
        {
	switch (row[3][0])
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
	        errAbort("Unknown type '%s' for %s", row[3], row[0]);
		break;
	    }
	}
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slSort(&liList, libInfoCmp);
return liList;
}

void sumStrandAndScore(struct estOrientInfo *eiList, int *retStrand, int *retScore)
/* Return sum of strand and score. */
{
struct estOrientInfo *ei;
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

void printGroupStats(FILE *f, struct estOrientInfo *eiList)
/* Print stats from list. */
{
int splicePlus=0, spliceMinus=0, signalPlus=0, signalMinus=0, polyPlus=0, polyMinus=0;
int all = 0, orient = 0, strand;
struct estOrientInfo *ei;

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


#ifdef NEEDSWORK
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
#endif /* NEEDSWORK */

void printLibStats(struct libInfo *liList, char *fileName)
/* Write stats on libs to file. */
{
struct libInfo *li;
FILE *f = mustOpen(fileName, "w");

#ifdef NEEDSWORK
fprintf(f, "#ests\tsize\tstartDate\tendngDate\t");
#else
fprintf(f, "#ests\t");
#endif /* NEEDSWORK */
labelGroup(f, "3'");
labelGroup(f, "5'");
labelGroup(f, "?");
fprintf(f, "library\tauthor\n");

for (li = liList; li != NULL; li = li->next)
    {
#ifdef NEEDSWORK
    char *minDate = NULL, *maxDate = NULL;
    long totalSize = 0;
    int count = 0, avgSize = 0;
#endif /* NEEDSWORK */

    if (li->estCount == 0)
        continue;
    fprintf(f, "%d\t", li->estCount);
#ifdef NEEDSWORK
    addSizeAndDates(li->threePrime, &totalSize, &count, &minDate, &maxDate);
    addSizeAndDates(li->fivePrime, &totalSize, &count, &minDate, &maxDate);
    addSizeAndDates(li->unPrime, &totalSize, &count, &minDate, &maxDate);
    if (count > 0)
        avgSize = totalSize/count;
    fprintf(f, "%d\t%s\t%s\t", avgSize, minDate, maxDate);
#endif /* NEEDSWORK */
    printGroupStats(f, li->threePrime);
    printGroupStats(f, li->fivePrime);
    printGroupStats(f, li->unPrime);
    fprintf(f, "%s\t", li->libName);
    fprintf(f, "%s\n", li->author);
    }
carefulClose(&f);
}

#ifdef NEEDSWORK
void addSizes(char *database, struct hash *eiHash, struct hash *dateHash)
/* Add and date info to ests in eiHash. Date strings are stored in dateHash */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr = NULL;
char **row;

sr = sqlGetResult(conn, NOSQLINJ "select acc,size,gb_date from seq");
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
#endif /* NEEDSWORK */

void estLibStats(char *database, char *eiInfoBed, char *output)
/* estLibStats - Calculate some stats on EST libraries given file from polyInfo. */
{
struct hash *eiHash = NULL, *liHash = newHash(0);
struct hash *libHash = NULL, *authorHash = NULL;
#ifdef NEEDSWORK
struct hash *dateHash = newHash(0);
#endif /* NEEDSWORK */
struct libInfo *liList = NULL;

libHash = readIdHash(database, "library");
authorHash = readIdHash(database, "author");
printf("Read libs and authors\n");
eiHash = readEstInfo(eiInfoBed);
printf("Read %s\n", eiInfoBed);
#ifdef NEEDSWORK
addSizes(database, eiHash, dateHash);
#endif /* NEEDSWORK */
printf("Added size info\n");
liList = addEsts(database, eiHash, libHash, authorHash, liHash);
printf("Read in 3' and other info from %s\n", database);
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
