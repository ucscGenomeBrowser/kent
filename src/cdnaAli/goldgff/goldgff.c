/* geniecon - build a list of Genie constrains from cDNA. */
#include "common.h"
#include "cda.h"
#include "snof.h"
#include "wormdna.h"

struct snof *aliSnof;
FILE *aliFile;

char **chromNames;
int chromCount;
FILE **conFiles;

void findStartEndWithRc(struct wormCdnaInfo *info, struct cdaAli *ali, struct dnaSeq *cdna, 
    int *retStart, int *retEnd)
/* Find start and end of coding region, reverse complementing if necessary. */
{
int start = info->cdsStart, end = info->cdsEnd;

if (ali->strand == '-')
    {
    int cdnaSize = cdna->size;
    int oldStart = start;
    start = cdnaSize - end + 1;
    end = cdnaSize - oldStart + 1;
    }
*retStart = start;
*retEnd = end;
}

int needleToHayIx(struct cdaAli *ali, int needleIx)
/* Convert index into needle into index into hay. */
{
struct cdaBlock *b = ali->blocks;
int bCount = ali->blockCount;
int i;
int offset;

/* First try to find it within an aligned area. */
for (i=0; i<bCount; ++i)
    {
    if (needleIx >= b->nStart && needleIx < b->nEnd)
        {
        offset = needleIx - b->nStart;
        return offset + b->hStart;
        }
    b += 1;
    }

/* Before first block? Warn and fake. */
b = ali->blocks;
if (needleIx < b->nStart)
    {
    warn("in %s needleIx %d before first block start %d", ali->name, needleIx, b->nStart);
    offset = needleIx - b->nStart;
    return offset + b->hStart;
    }

/* After last block? */
b = ali->blocks + bCount - 1;
if (needleIx >= b->nEnd)
    {
    warn("in %s (strand %c) needleIx %d after last block end %d", 
        ali->name, ali->strand, needleIx, b->nEnd);
    offset = needleIx - b->nStart;
    return offset + b->hStart;
    }

/* Must be in-between blocks */
b = ali->blocks;
for (i=1; i<bCount; ++i)
    {
    if (needleIx < b[1].nStart)
        {
        warn("in %s (strand %c) needleIx %d between block end %d and next block start %d",
            ali->name, ali->strand, needleIx, b[0].nEnd, b[1].nStart);
        offset = needleIx = b->nStart;
        return offset + b->hStart;
        }
    b += 1;
    }
assert(FALSE);
return 0;
}

void writeFullConstraints(struct wormCdnaInfo *info, struct cdaAli *ali, struct dnaSeq *cdna,
    char *group, FILE *out)
/* Write what constraints you can to file. */
{
char *chrom = chromNames[ali->chromIx];
static char strand[2];
int nucIx = 0;
int frame;
int i;
struct cdaBlock *b = ali->blocks;
int bCount = ali->blockCount;
int start, end;
int cdsStart, cdsEnd;
boolean wroteLast = FALSE;
boolean isRc = (ali->strand == '-');

//if (bCount == 1)
//    uglyf("%s is a one exon gene on strand %c of chrom %s\n", group, ali->strand, chrom);

strand[0] = ali->strand;
findStartEndWithRc(info, ali, cdna, &cdsStart, &cdsEnd);

{
if (isRc)
    {
    if (b->nEnd < cdsStart)
        {
        static reported;
        if (!reported)
            {
            reported = TRUE;
            uglyf("%s (%s) ends before last exon strand %c of chrom %s\n", 
                ali->name, group, ali->strand, chrom);
            }
        }
    if (b[bCount-1].nStart > cdsEnd)
        {
        static reported;
        if (!reported)
            {
            reported = TRUE;
            uglyf("%s (%s) begins after first exon strand %c of chrom %s\n", 
                ali->name, group, ali->strand, chrom);
            }
        }
    }
else
    {
    if (b->nEnd < cdsStart)
        {
        static reported;
        if (!reported)
            {
            reported = TRUE;
            uglyf("%s (%s) begins after first exon strand %c of chrom %s\n", 
                ali->name, group, ali->strand, chrom);
            }
        }
    if (b[bCount-1].nStart > cdsEnd)
        {
        static reported;
        if (!reported)
            {
            reported = TRUE;
            uglyf("%s (%s) ends before last exon strand %c of chrom %s\n", 
                ali->name, group, ali->strand, chrom);
            }
        }
    }
}

/* Write out start codon. */
start = needleToHayIx(ali, cdsStart-1)+1;
fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
    chrom, ali->name, (isRc ? "stop_codon" : "start_codon"),
    start, start+2,
    ".", strand, 0, group);

/* Find first coding exon and write it out. */
for (i=0; i<bCount; ++i)
    {
    if (b->nEnd > cdsStart-1)
        break;
    ++b;
    }
if (i == bCount-1)
    {
    end = needleToHayIx(ali, cdsEnd-1);
    wroteLast = TRUE;
    }
else
    end = b->hEnd;
fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
    chrom, ali->name, "CDS",
    start, end,
    ".", strand, 0, group);
if (i != bCount-1)
    {
    fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t.\t%s\n",
        chrom, ali->name, "splice5",
        b->hEnd, b->hEnd+1,
        ".", strand, group);
    nucIx += (end-start);
    ++i;
    ++b;
    }

end = needleToHayIx(ali, cdsEnd-1);

/* Write out the middle codons. */
for (; i<bCount-1; ++i)
    {
    if (b->hEnd >= end)
        break;
    frame = nucIx%3;
    fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t.\t%s\n",
        chrom, ali->name, "splice3",
        b->hStart, b->hStart+1,
        ".", strand, group);
    fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
        chrom, ali->name, "CDS",
        b->hStart+1, b->hEnd,
        ".", strand, frame, group);
    fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t.\t%s\n",
        chrom, ali->name, "splice5",
        b->hEnd, b->hEnd+1,
        ".", strand, group);
    fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t.\t%s\n",
        chrom, ali->name, "intron",
        b[0].hEnd+1, b[1].hStart,
        ".", strand, group);
    nucIx += (b->hEnd - b->hStart);
    ++b;
    }

/* Write out last exon. */
if (!wroteLast)
    {
    frame = nucIx%3;
    fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t.\t%s\n",
        chrom, ali->name, "splice3",
        b->hStart, b->hStart+1,
        ".", strand, group);
    fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
        chrom, ali->name, "CDS",
        b->hStart+1, end,
        ".", strand, frame, group);
    }

/* Write out stop codon. */
end = needleToHayIx(ali, cdsEnd-1);
fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
    chrom, ali->name, (isRc ? "start_codon" : "stop_codon"),
    end-2, end,
    ".", strand, 0, group);
}

boolean reallyGoodAlignment(struct wormCdnaInfo *info, struct cdaAli *ali, struct dnaSeq *cdna)
/* Return TRUE if have solid ends everywhere and alignment
 * throughout coding region. */
{
struct cdaBlock *b = ali->blocks;
int bCount = ali->blockCount;
int i;
int start, end;

findStartEndWithRc(info, ali, cdna, &start, &end);
if (b[0].nStart >= start)
    return FALSE;
if (b[bCount-1].nEnd < end)
    return FALSE;
for (i=1; i<bCount; ++i)
    {
    if (b[0].nEnd != b[1].nStart)
        return FALSE;
    if (b[1].hStart - b[0].hEnd < 12)
        return FALSE;
    b += 1;
    }
return TRUE;
}

int main(int argc, char *argv[])
{
char fileName[512];
char *outDir;
FILE *cdiFile;
char cdiLine[1024];
int lineCount = 0;
struct wormCdnaInfo info;
char *cdnaName;
long offset;
struct cdaAli *ali;
char *idName;
int chromIx;
int goodAliCount = 0;
struct dnaSeq *cdna;

/* Check usage and abort if bad. */                
if (argc != 2)
    {
    errAbort("geniecon - Create constraint files for Genie the genefinder based\n"
             "cDNA alignments.\n"
             "Usage:\n"
             "    geniecon outputDir\n"
             "This will create the files cI.gff, cII.gff and so on (one for each\n"
             "chromosome) in the outputDir directory.");
    }
outDir = argv[1];

/* Open up alignment file and index. */
sprintf(fileName, "%sgood", wormCdnaDir());
aliSnof = snofOpen(fileName);
aliFile = wormOpenGoodAli();

/* Open up output files. */
wormChromNames(&chromNames, &chromCount);
conFiles = needMem(chromCount*sizeof(FILE*));
for (chromIx = 0; chromIx<chromCount; ++chromIx)
    {
    char upcChrom[19];
    strcpy(upcChrom, chromNames[chromIx]);
    touppers(upcChrom);
    sprintf(fileName, "%s/gold%s.gff", outDir, upcChrom);
    conFiles[chromIx] = mustOpen(fileName, "w");
    }

/* Loop through all cDNAs extracting constraint info */
sprintf(fileName, "%s%s", wormCdnaDir(), "allcdna.cdi");
cdiFile = mustOpen(fileName, "rb");
while (fgets(cdiLine, sizeof(cdiLine), cdiFile))
    {
    ++lineCount;
    if (lineCount%10000 == 0)
        printf("cdna #%d\n", lineCount);
    wormFaCommentIntoInfo(cdiLine, &info);
    idName = cdnaName = cdiLine+1;
    if (info.description && info.knowStart && info.knowEnd)
        {
        if (info.gene)
            idName = info.gene;
        if (!snofFindOffset(aliSnof, cdnaName, &offset))
            continue;
        fseek(aliFile, offset, SEEK_SET);
        ali = cdaLoadOne(aliFile);
        if (!wormCdnaSeq(cdnaName, &cdna, NULL))
            errAbort("Couldn't load cDNA %s", cdnaName);
        if (reallyGoodAlignment(&info, ali, cdna))
            {
            writeFullConstraints(&info, ali, cdna, idName, conFiles[ali->chromIx]);
            goodAliCount += 1;
            }
        freeDnaSeq(&cdna);
        cdaFreeAli(ali);
        }
    }
printf("%d really good alignments of full length cDNAs\n", goodAliCount);
return 0;
}
