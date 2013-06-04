/* encodeSynteny - create HTML files to compare syntenic predictions from liftOver and Mercator */
#include "regionOrtho.h"

void usage()
{
errAbort("regionOrtho - merge orthology predictions from liftOver and Mercator.\n"
	 "              generates BED files for the regions. \n"
	 "              (inputs in BED 4 files or BED 4+ tables)\n"
	 "usage:\n\tregionOrtho { [sourceDb.]sourceTable | [sourcePath/]sourceFile] } \\\n"
	 "                    { [toDb.]orthoTable1     | [orthoPath1/]orthoFile1  } \\\n"
	 "                    { [toDb.]orthoTable2     | [orthoPath2/]orthoFile2  } \\\n"
	 "                    consensusFile.bed order.err\n");
}

struct sizeList *getRegions(char *regionSource, boolean excludeRandoms)
{
struct sizeList *list = NULL, *sl;
if (fileExists(regionSource))
    {
    struct lineFile *IN = lineFileOpen(regionSource, TRUE);
    char *row[4];
    while (lineFileRow(IN, row))
	{
	if (startsWith(row[3], "MEN"))
	    continue;
	if (sameString(row[0], "chrom"))
	    continue;
	if (excludeRandoms && (startsWith(row[0], "chrUn") || endsWith(row[0], "random")))
	    continue;
	AllocVar(sl);
	sl->chrom      = cloneString(row[0]);
	sl->chromStart = atoi(row[1]);
	sl->chromEnd   = atoi(row[2]);
	sl->name       = strndup(row[3],6);
	sl->size       = atoi(row[2])-atoi(row[1]);
	slAddHead(&list, sl);
	}
    lineFileClose(&IN);
    }
else
    {
    struct sqlConnection *conn = sqlConnect("hg17");
    char query[1024];
    char **row;
    struct sqlResult *sr = NULL;
    
    sqlSafef(query, sizeof(query), 
	  "select distinct chrom, chromStart, chromEnd, name "
	  "from %s order by name", regionSource);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (sameString(row[0], "chrom"))
	    continue;
	if (excludeRandoms && (startsWith(row[0], "chrUn") || endsWith(row[0], "random")))
	    continue;
	AllocVar(sl);
	sl->chrom      = cloneString(row[0]);
	sl->chromStart = atoi(row[1]);
	sl->chromEnd   = atoi(row[2]);
	sl->name       = strndup(row[3],6);
	sl->size       = atoi(row[2])-atoi(row[1]);
	slAddHead(&list, sl);
	}
    sqlFreeResult(&sr);
    sqlDisconnect(&conn);
    }
slReverse(&list);
return list;
}

struct sizeList *sizeListNew(char *chrom, int chromStart, int chromEnd, char *name)
/* Return new sizeList. */
{
struct sizeList *s = needMem(sizeof(*s));
s->chrom=strdup(chrom);
s->chromStart=chromStart;
s->chromEnd=chromEnd;
s->name=strdup(name);
return s;
}

struct sizeList *sizeListClone(struct sizeList *list)
/* Return clone of list. */
{
struct sizeList *el, *newEl, *newList = NULL;
for (el = list; el != NULL; el = el->next)
    {
    newEl = sizeListNew(el->chrom, el->chromStart, el->chromEnd, el->name);
    slAddHead(&newList, newEl);
    }
slReverse(&newList);
return newList;
}

struct sizeList *unionSizeLists(struct sizeList *a, struct sizeList *b, FILE *err)
{
struct sizeList *s, *t, *u, *c=sizeListClone(a), *d=sizeListClone(b);
boolean didChange=TRUE;
int mergeGaps=20000;
if (a == NULL)
    return b;
while (didChange)
    {
    didChange=FALSE;
    for (s = c; s != NULL; s = s->next)
	for (t = d; t != NULL; t = t->next)
	    {
	    if (t->chrom == NULL || t->name == NULL)
		continue;
//	    printf("%s/%s.%d-%d\t%s/%s.%d-%d\t", s->name, s->chrom, s->chromStart, s->chromEnd, t->name, t->chrom, t->chromStart, t->chromEnd);
	    if ( !strncmp(s->name, t->name, 6) && sameString(s->chrom, t->chrom) )
		if (rangeIntersection(s->chromStart,s->chromEnd,t->chromStart,t->chromEnd)+mergeGaps>0)
		    {
		    s->chromStart = min(s->chromStart,t->chromStart);
		    s->chromEnd   = max(s->chromEnd,  t->chromEnd);
		    t->chrom  = t->name = NULL; // it would be better to remove the element here
		    didChange = TRUE;
		    continue;
		    }
	    }
    }
for (t = d; t != NULL; t = t->next)
    if (t->name != NULL && t->chrom!=NULL)
	{
	u = sizeListNew(t->chrom, t->chromStart, t->chromEnd, t->name);
	slAddTail(c, u);
	fprintf(err, "%s\t%d\t%d\t%s\n", t->chrom, t->chromStart, t->chromEnd, t->name);
	}
return c;
}

void writeSizeListToBedFile(FILE *File, struct sizeList *sList)
{
struct sizeList *sl=NULL;
char *name;
for ( sl = sList; sl != NULL; sl = sl->next)
    {
    if (endsWith(sl->name,"+") || endsWith(sl->name,"-"))
	chopSuffixAt(sl->name, '_');
    fprintf(File, "%s\t%d\t%d\t%s\n", sl->chrom, sl->chromStart, sl->chromEnd, sl->name);
    }
}

int main(int argc, char *argv[])
{
char *ortho1;
char *ortho2;
char *consensus;
char *err;
struct sizeList *ortho1List=NULL;
struct sizeList *ortho2List=NULL;
struct sizeList *consensusList=NULL;
FILE *consensusFile=NULL;
FILE *errFile=NULL;

if(argc != 5)
    usage();

ortho1        = cloneString(argv[1]); // liftOver
ortho2        = cloneString(argv[2]); // Mercator
consensus     = cloneString(argv[3]); // Consensus
err           = cloneString(argv[4]); // errors

ortho1List    = getRegions(ortho1, FALSE); // liftOver - include random chroms
ortho2List    = getRegions(ortho2, TRUE ); // Mercator - exclude random chroms

consensusFile = mustOpen(consensus, "w");
errFile       = mustOpen(err, "w");

consensusList = unionSizeLists(ortho1List,    ortho2List, errFile);
consensusList = unionSizeLists(consensusList, ortho1List, errFile);
consensusList = unionSizeLists(consensusList, ortho2List, errFile);
writeSizeListToBedFile(consensusFile, consensusList);
return 0;
}

