/* gdf - Intronerator Gene Description File. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "gdf.h"


struct gdfGene *newGdfGene(char *name, int nameSize, int exonCount, char strand, UBYTE chromIx)
/* Return a new gene. */
{
struct gdfGene *gene = needMem(sizeof *gene);
gene->name = cloneStringZ(name, nameSize);
gene->dataCount = exonCount*2;
if (exonCount > 0)
    {
    gene->dataPoints = 
	    needMem(gene->dataCount * sizeof(gene->dataPoints[0]));
    }
gene->strand = strand;
gene->chromIx = chromIx;
return gene;
}

void gdfFreeGene(struct gdfGene *gene)
/* Free a gene. */
{
if (gene != NULL)
    {
    freeMem(gene->name);
    freeMem(gene->dataPoints);
    freeMem(gene);
    }
}

void gdfFreeGeneList(struct gdfGene **pList)
/* Free a whole list of genes. */
{
struct gdfGene *gene, *next;
gene = *pList;
while (gene != NULL)
    {
    next = gene->next;
    gdfFreeGene(gene);
    gene = next;
    }
*pList = NULL;
}

struct gdfGene *gdfReadOneGene(FILE *f)
/* Read one entry from a Gdf file.  Assumes that the file pointer
 * is in the right place. */
{
short pointCount;
char strand;
UBYTE geneNameSize, chromIx;
char geneNameBuf[128];
struct gdfGene *gene;

mustReadOne(f, geneNameSize);
mustRead(f, geneNameBuf, geneNameSize);
geneNameBuf[geneNameSize] = 0;
mustReadOne(f, chromIx);
mustReadOne(f, strand);
mustReadOne(f, pointCount);
gene = newGdfGene(geneNameBuf, geneNameSize, pointCount>>1, strand, chromIx);
mustRead(f, gene->dataPoints, sizeof(gene->dataPoints[0]) * pointCount);
return gene;
}

void gdfGeneExtents(struct gdfGene *gene, long *pMin, long *pMax)
/* Figure out first and last base in gene. */
{
int i;
long x;
long min=0x7000000;
long max = -min;

for (i=0; i<gene->dataCount; i+=1)
    {
    x = gene->dataPoints[i].start;
    if (x < min)
	min = x;
    if (x > max)
	max = x;
    }
*pMin = min;
*pMax = max;
}

void gdfOffsetGene(struct gdfGene *gene, int offset)
/* Add offset to each point in gene */
{
struct gdfDataPoint *dp = gene->dataPoints;
int count = gene->dataCount;
int i;
for (i=0; i<count; ++i)
    dp[i].start += offset;
}

void gdfRcGene(struct gdfGene *gene, int size)
/* Flip gene to other strand. Assumes dataPoints are already
 * moved into range from 0-size */
{
struct gdfDataPoint *s = gene->dataPoints, *e, temp;
int count = gene->dataCount;
int i;
int halfCount = count/2;


for (i=0; i<count; ++i)
    {
    s->start = reverseOffset(s->start, size) + 1;
    ++s;
    }
s = gene->dataPoints;
e = s + gene->dataCount-1;
for (i=0; i<halfCount; i += 1)
    {
    memcpy(&temp, s, sizeof(temp));
    memcpy(s, e, sizeof(temp));
    memcpy(e, &temp, sizeof(temp));
    s += 1;
    e -= 1;
    }
}


void gdfUpcExons(struct gdfGene *gene, int geneOffset, DNA *dna, int dnaSize, int dnaOffset)
/* Uppercase exons in DNA. */
{
struct gdfDataPoint *dp = gene->dataPoints;
int count = gene->dataCount;
int start, end;
long gffStart, gffEnd;
int combinedOffset;
int i;

gdfGeneExtents(gene, &gffStart, &gffEnd);
combinedOffset = -gffStart + geneOffset - dnaOffset;
for (i=0; i<count; i += 2)
    {
    start = dp[i].start + combinedOffset;
    end = dp[i+1].start + combinedOffset;
    if (end <= 0 || start >= dnaSize)
        continue;
    if (start < 0) start = 0;
    if (end > dnaSize) end = dnaSize;
    toUpperN(dna+start, end-start);
    }
}

