/* ldHgGene - load a set of Genie gene predictions from GFF file into
 * mySQL database. */

#include "common.h"
#include "hash.h"
#include "gff.h"
#include "hgap.h"
#include "hgGene.h"


void usage()
{
errAbort(
    "ldHgGene - load database with gene predictions from a gff file.\n"
    "usage:\n"
    "     ldHgGene file(s).gff");
}

void getSourceIds(struct gffFile *gff, struct sqlConnection *conn)
/* Fill in source ids from database. If not in database put them 
 * there. */
{
struct gffSource *source;
HGID id;

char query[256];
for (source = gff->sourceList; source != NULL; source = source->next)
    {
    sprintf(query, "select id from geneFinder where name='%s'", source->name);
    if ((id = sqlQuickNum(conn, query)) == 0)
	{
	id = hgNextId();
	sprintf(query, "insert into geneFinder VALUES(%lu,'%s')", id, source->name);
	sqlUpdate(conn, query);
	}
    source->id = id;
    }
}

void makeGeneName(char *tnName, char *geneName)
/* Make base gene name from transcript name. */
{
char *e;
strcpy(geneName, tnName);
//e = strrchr(geneName, '-');
//*e = 0;
}

struct gffGene
/* This contains info on a gene (a group of transcripts) */
    {
    struct gffGene *next;	/* Next in list. */
    char *name;			/* Gene name. Not alloced here. */
    struct gffGroup *transcriptList; /* List of transcripts. */
    };

void gffGeneFree(struct gffGene **pObj)
/* Free one gff gene. */
{
struct gffGene *obj;
if ((obj = *pObj) != NULL)
    {
    gffGroupFreeList(&obj->transcriptList);
    freez(pObj);
    }
}

void gffGeneFreeList(struct gffGene **pList)
/* Free up a list of gffGenes. */
{
struct gffGene *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gffGeneFree(&el);
    }
*pList = NULL;
}


struct gffGene *groupTranscripts(struct gffFile *gff, struct hash *geneHash)
/* Each group in this file represents a transcript.  This
 * groups together transcripts in the same gene.   It cannibalizes
 * gff->groupList in the process, moving it to gffGene->transcriptList. */
{
struct gffGroup *gg, *ggNext;
struct gffGene *gene, *geneList = NULL;
char geneName[256];
struct hashEl *hel;

for (gg = gff->groupList; gg != NULL; gg = ggNext)
    {
    ggNext = gg->next;
    makeGeneName(gg->name, geneName);
    if ((hel = hashLookup(geneHash, geneName)) == NULL)
	{
	AllocVar(gene);
	hel = hashAdd(geneHash, geneName, gene);
	gene->name = hel->name;
	slAddHead(&geneList, gene);
	}
    else
	{
	gene = hel->val;
	}
    slAddHead(&gene->transcriptList, gg);
    }
slReverse(&geneList);
for (gene = geneList; gene != NULL; gene = gene->next)
    slReverse(&gene->transcriptList);
return geneList;
}

void findExtentFromTranscripts(struct gffGroup *transcriptList, struct hgGene *gene)
/* Fill in start, end, etc fields of gene from transcript list. */
{
struct gffGroup *transcript = transcriptList;
int start = 0x3ffffff;
int end = -start;
gene->startBac = gene->endBac = hgGetBac(transcript->seq)->id;
gene->orientation = (transcript->strand == '-' ? -1 : 1);
for (;transcript != NULL; transcript = transcript->next)
    {
    if (start > transcript->start)
	start = transcript->start;
    if (end < transcript->end)
	end = transcript->end;
    }
gene->startPos = start;
gene->endPos = end;
}

void getExons(struct gffGroup *group, struct hgTranscript *tn)
/* Translate exons from gffGroup format to hgTranscript format. */
{
int exonCount = 0;
int exonIx;
HGID bacId = tn->startBac;    /* Will have to update with virtual contigs... */
unsigned *exonStartBacs;/* Exon start positions */
unsigned *exonStartPos;	/* Exon start positions */
unsigned *exonEndBacs;	/* Exon start positions */
unsigned *exonEndPos;	/* Exon start positions */
struct gffLine *line;

for (line = group->lineList; line != NULL; line = line->next)
    {
    if (sameString(line->feature, "exon") || sameString(line->feature, "CDS"))
	{
	++exonCount;
	}
    }
tn->exonCount = exonCount;
tn->exonStartBacs = AllocArray(exonStartBacs, exonCount);
tn->exonStartPos = AllocArray(exonStartPos, exonCount);
tn->exonEndBacs = AllocArray(exonEndBacs, exonCount);
tn->exonEndPos = AllocArray(exonEndPos, exonCount);
exonIx = 0;
for (line = group->lineList; line != NULL; line = line->next)
    {
    if (sameString(line->feature, "exon") || sameString(line->feature, "CDS"))
	{
	exonStartBacs[exonIx] = exonEndBacs[exonIx] = bacId;
	exonStartPos[exonIx] = line->start;
	exonEndPos[exonIx] = line->end;
	++exonIx;
	}
    }
}

int loadGff(char *gffName, struct sqlConnection *conn, FILE *geneTab, FILE *tnTab)
/* Load one gff file into database. Returns number of genes loaded. */
{
struct gffFile *gff;
int lineCount;
int geneCount;
struct gffGroup *gg;
struct hgGene *geneList = NULL;
struct hash *geneHash = newHash(12);
struct gffGene *gffGeneList, *gffGene;

gff = gffRead(gffName);
lineCount = slCount(gff->lineList);
gffGroupLines(gff);
geneCount = slCount(gff->groupList);
printf("%s has %d lines %d groups %d seqs %d sources %d feature types\n",
    gffName, lineCount, slCount(gff->groupList), slCount(gff->seqList), slCount(gff->sourceList),
    slCount(gff->featureList));


getSourceIds(gff, conn);
gffGeneList = groupTranscripts(gff, geneHash);
for (gffGene = gffGeneList; gffGene != NULL; gffGene = gffGene->next)
    {
    struct gffGroup *group = gffGene->transcriptList;
    int tnCount = slCount(group);
    struct hgGene *gene;
    struct gffSource *source;
    HGID geneId;
    HGID *transcriptIds;
    int ix;

    AllocVar(gene);
    gene->id = geneId = hgNextId();
    gene->name = cloneString(gffGene->name);
    findExtentFromTranscripts(group, gene);
    gene->transcriptCount = tnCount;
    gene->transcripts = AllocArray(transcriptIds, tnCount);
    source = hashLookup(gff->sourceHash, group->source)->val;
    gene->geneFinder = source->id;
    for (ix=0; group != NULL; group = group->next, ++ix)
	{
	struct hgTranscript *tn;
	AllocVar(tn);
	tn->id = transcriptIds[ix] = hgNextId();
	tn->name = cloneString(group->name);
	tn->hgGene = geneId;
	tn->startBac = tn->endBac = tn->cdsStartBac = tn->cdsEndBac = gene->startBac;
	tn->startPos = tn->cdsStartPos = group->start;
	tn->endPos = tn->cdsEndPos = group->end;
	tn->orientation = (group->strand == '-' ? -1 : 1);
	getExons(group, tn);
	hgTranscriptTabOut(tn, tnTab);
	}
    hgGeneTabOut(gene, geneTab);
    }
gffFileFree(&gff);
freeHash(&geneHash);
return geneCount;
}


void loadGffs(char *gffNames[], int gffCount)
/* Load in a series of gff files. */
{
int i;
int geneCount = 0;
struct sqlConnection *conn;
FILE *geneTab = hgCreateTabFile("hgGene");
FILE *tnTab = hgCreateTabFile("hgTranscript");

conn = hgStartUpdate();
for (i=0; i<gffCount; ++i)
    {
    printf("Processing %s\n", gffNames[i]);
    geneCount += loadGff(gffNames[i], conn, geneTab, tnTab);
    }
fclose(geneTab);
fclose(tnTab);
printf("Loading %d genes into database\n", geneCount);
hgLoadTabFile(conn, "hgGene");
hgLoadTabFile(conn, "hgTranscript");
hgEndUpdate(&conn, "ldHgGene %d genes in %d GFF files including %s", geneCount, gffCount, gffNames[0]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
loadGffs(argv+1, argc-1);
}
