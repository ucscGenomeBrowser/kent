/* coorXlat - Translates between Ensembl contig, UCSC contig, and
 * flatFile sequence coordinates.  Hopefully this is a temporary
 * measure, at least as far as the Ensembl contigs and the UCSC ones
 * go. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "ens.h"
#include "hgdb.h"

struct hash *accHash;		/* Hash of all clones by accession. */
struct hash *ensContigHash;	/* Has of Ensembl contigs by ensemble ID. */
struct cloneInfo *cloneList;	/* List of all clones. */

struct cloneInfo
/* Information about a clone from both UC and Ensembl.  
 * This is the hel->val data type of the accHash. */
    {
    struct cloneInfo *next;		/* Next in list. */
    char *acc;				/* Accession # of clone. */
    struct ensCloneInfo *ens;		/* Ensembl info. */
    struct ucCloneInfo *uc;		/* UC info. */
    };

struct ensContigInfo
/* Information about a single contig. */
    {
    struct ensContigInfo *next;		/* Next in list. */
    char *id;				/* Id. Not stored here. */
    int length;				/* Length of contig. */
    int offset;				/* Offset within clone. */
    int orientation;			/* Ensembl's best guess orientation. */
    int corder;				/* Ensembl's best guess ordering. */
    };

struct ensCloneInfo
/* Information about a clone. */
    {
    struct ensContigInfo *contigList;
    };

struct ucContigInfo
/* Info about a single contig. */
    {
    struct ucContigInfo *next;		/* Next in list. */
    int length;				/* Length of contig. */
    int offset;				/* Offset within clone. */
    };

struct ucCloneInfo
/* Information about a UC clone. */
    {
    struct ucContigInfo *contigList;
    };

void printAll(char *fileName)
/* Print all clones. */
{
struct cloneInfo *ci;
struct ucCloneInfo *uci;
struct ensCloneInfo *eci;
FILE *f = mustOpen(fileName, "w");
slReverse(&cloneList);
for (ci = cloneList; ci != NULL; ci = ci->next)
    {
    fprintf(f, "%s\n", ci->acc);
    if (eci = ci->ens)
	{
	struct ensContigInfo *ci;
	fprintf(f, "en ");
	for (ci = eci->contigList; ci != NULL; ci = ci->next)
	    fprintf(f, "%d-%d ", ci->offset, ci->offset+ci->length);
	fprintf(f, "\n");
	}
    if (uci = ci->uc)
	{
	struct ucContigInfo *ci;
	fprintf(f, "uc ");
	for (ci = uci->contigList; ci != NULL; ci = ci->next)
	    fprintf(f, "%d-%d ", ci->offset, ci->offset+ci->length);
	fprintf(f, "\n");
	}
    }
fclose(f);
}

void loadEnsembl()
/* Load up ensemble info. */
{
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
int cloneCount = 0, contigCount = 0;
long start,end;

accHash = newHash(16);
ensContigHash = newHash(19);
start = clock1000();
conn = sqlConnect("ensdev");
sr = sqlGetResult(conn, "SELECT id,clone,length,offset,orientation,corder FROM contig");
end = clock1000();
printf("  sqlConnect and GetResult in %4.3f seconds\n", 0.001*(end-start));
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *contigId = row[0];
    char *cloneName = row[1];
    struct hashEl *hel;
    struct cloneInfo *cloneInfo;
    struct ensCloneInfo *eci;
    struct ensContigInfo *contig;
    if ((hel = hashLookup(accHash, cloneName)) == NULL)
	{
	AllocVar(cloneInfo);
	++cloneCount;
	hel = hashAdd(accHash, cloneName, cloneInfo);
	cloneInfo->acc = hel->name;
	AllocVar(eci);
	cloneInfo->ens = eci;
	slAddHead(&cloneList, cloneInfo);
	}
    else
	{
	cloneInfo = hel->val;
	eci = cloneInfo->ens;
	}
    ++contigCount;
    AllocVar(contig);
    hel = hashAdd(ensContigHash, contigId, contig);
    contig->id = hel->name;
    contig->length = sqlUnsigned(row[2]);
    contig->offset = sqlSigned(row[3]);
    contig->orientation = sqlSigned(row[4]);
    contig->corder = sqlUnsigned(row[5]);
    slAddTail(&eci->contigList, contig);
    }
sqlDisconnect(&conn);
printf("Loaded %d contigs in %d clones\n", contigCount, cloneCount);
}

struct ucContigInfo *ucParseContig(char *line, int lineSize)
/* Make list of contigs from line that looks like:
 *    0-1200 1300-1889 1989-10022 
 * Should be easy.... */
{
char *words[256];
int wordCount;
int i;
char *s, *e;
struct ucContigInfo *list = NULL, *el;
int x1,x2;

wordCount = chopLine(line, words);
for (i = 0; i<wordCount; ++i)
    {
    s = words[i];
    e = strchr(s, '-');
    *e++ = 0;
    AllocVar(el);
    x1 = sqlUnsigned(s);
    x2 = sqlUnsigned(e);
    el->offset = x1;
    el->length = x2-x1;
    slAddHead(&list, el);
    }
slReverse(&list);
return list;
}

void loadFromRa(char *raName, boolean finished)
/* Load up UC info from ra file. */
{
struct lineFile *lf = lineFileOpen(raName, TRUE);
char *line;
int lineSize;
char acc[32];
boolean isOpen = FALSE;
struct ucContigInfo *contigList = NULL;
int cloneCount = 0, contigCount = 0;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (startsWith("acc ", line))
	{
	memcpy(acc, line+4, lineSize-4);
	isOpen = TRUE;
	}
    else if (startsWith("ctg ", line))
	{
	contigList = ucParseContig(line+4, lineSize-4);
	contigCount += slCount(contigList);
	}
    else if (line[0] == 0)
	{
	struct hashEl *hel;
	struct cloneInfo *cloneInfo;
	struct ucCloneInfo *uci;
	if ((hel = hashLookup(accHash, acc)) == NULL)
	    {
	    AllocVar(cloneInfo);
	    ++cloneCount;
	    hel = hashAdd(accHash, acc, cloneInfo);
	    cloneInfo->acc = hel->name;
	    slAddHead(&cloneList, cloneInfo);
	    }
	else
	    cloneInfo = hel->val;
	AllocVar(uci);
	uci->contigList = contigList;
	cloneInfo->uc = uci;
	contigList = NULL;
	}
    }
lineFileClose(&lf);
printf("Loaded %d contigs in %d clones in %s\n", contigCount, cloneCount, raName);
}

void loadUc()
/* Load up UC info. */
{
char *dbDir = hgdbRootDir();
char finRaName[512];
char unfinRaName[512];
sprintf(unfinRaName, "%s%s", dbDir, "unfin/unfin.ra");
loadFromRa(unfinRaName, FALSE);
sprintf(finRaName, "%s%s", dbDir, "fin/fin.ra");
loadFromRa(finRaName, TRUE);
}

void cxlInitEnsUc()
/* Initialize coordinate translation info. */
{
boolean initted = FALSE;
long start, end;
if (!initted)
    {
    printf("loadEnsembl.....\n");
    start = clock1000();
    loadEnsembl();
    end = clock1000();
    printf("  loaded in %4.3f seconds\n", 0.001*(end-start));
    printf("loadingUC.....\n");
    start = clock1000();
    loadUc();
    end = clock1000();
    printf("  loaded in %4.3f seconds\n", 0.001*(end-start));
    printf("done init.\n");
    initted = TRUE;
    }
}

void cxlUcToEns(char *accession, int ucContig, int ucX, int *retEnsContig, int *retEnsX)
/* Translate from UC to Ensemble coordinates. */
{
}

void cxlEnsToUc(char *accession, int ensContig, int ensX, int *retUcContig, int *retUcX)
/* Translate from Ensemble to UC coordinates. */
{
}

int cxlUcToFlat(char *accession, int ucContig, int ucX)
/* Translate from UC to flat coordinates. */
{
}

int cxlEnsToFlat(char *accession, int ensContig, int ensX)
/* Translate from Ensemble to flat coordinates. */
{
}

void cxlFlatToUc(char *accession, int flatX, int *retUcContig, int *retUcX)
/* Translate from flat to UC coordinates. */
{
}

void cxlFlatToEns(char *accession, int flatX, int *retEnsContig, int *retEnxX)
/* Translate from flat to Ensemble coordinates. */
{
}

int main(int argc, char *argv[])
{
printf("Starting...\n");
cxlInitEnsUc();
printf("Printing...\n");
printAll("foo");
return 0;
}
