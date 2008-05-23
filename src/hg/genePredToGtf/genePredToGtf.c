/* genePredToGtf - Convert genePred table or file to gtf.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "genePred.h"
#include "genePredReader.h"

static char const rcsid[] = "$Id: genePredToGtf.c,v 1.11 2008/05/23 21:49:00 markd Exp $";

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "genePredToGtf - Convert genePred table or file to gtf.\n"
  "usage:\n"
  "   genePredToGtf database genePredTable output.gtf\n"
  "If database is 'file' then track is interpreted as a file\n"
  "rather than a table in database.\n"
  "options:\n"
  "   -utr - Add 5UTR and 3UTR features\n"
  "   -stopBetweenCdsUtr - put the stop_codon feature between the CDS\n"
  "    and 3'UTR.  To support broken programs that don't follow the spec\n"
  "    which puts it in the 3'UTR.\n"
  "   -honorCdsStat - use cdsStartStat/cdsEndStat when defining start/end\n"
  "    codon records\n"
  "   -source=src set source name to uses\n"
  "Note: use refFlat or extended genePred table to include geneName\n"
  );
}

static struct optionSpec options[] = {
   {"utr", OPTION_BOOLEAN},
   {"stopBetweenCdsUtr", OPTION_BOOLEAN},
   {"honorCdsStat", OPTION_BOOLEAN},
   {"source", OPTION_STRING},
   {NULL, 0}
};
static boolean utr = FALSE;
static boolean stopBetweenCdsUtr = FALSE;
static boolean honorCdsStat = FALSE;
static char *source = NULL;

static char *findUniqueName(struct hash *dupeHash, char *root)
/* If root name is already in hash, return root_1, root_2
 * or something like that. */
{
struct hashEl *hel;
if ((hel = hashLookup(dupeHash, root)) == NULL)
    {
    hashAddInt(dupeHash, root, 1);
    return root;
    }
else
    {
    static char buf[256];
    int val = ptToInt(hel->val) + 1;
    hel->val = intToPt(val);
    safef(buf, sizeof(buf), "%s_%d", root, val);
    return buf;
    }
}

static void writeGtfLine(FILE *f, char *source, char *name, char *geneName,
	char *chrom, char strand, char *type, 
	int start, int end, int exonIx, int frame)
/* Write a single exon to GTF file */
{
assert(start <= end);
/* convert frame to phase */
char phase = (frame < 0) ? '.' : (frame == 0) ? '0'
    : (frame == 1) ? '2' : '1';
fprintf(f, "%s\t", chrom);
fprintf(f, "%s\t", source);
fprintf(f, "%s\t", type);
fprintf(f, "%d\t", start+1);
fprintf(f, "%d\t", end);
fprintf(f, ".\t");	/* Score. */
fprintf(f, "%c\t", strand);
fprintf(f, "%c\t", phase);
fprintf(f, "gene_id \"%s\"; ", name);
fprintf(f, "transcript_id \"%s\"; ", name);
fprintf(f, "exon_number \"%d\"; ", exonIx+1);
fprintf(f, "exon_id \"%s.%d\";", name, exonIx+1);
if ((geneName != NULL) && (strlen(geneName)>0))
    fprintf(f, " gene_name \"%s\";", geneName);
fprintf(f, "\n");
}

struct codonCoords
/* coordinates of a codon */
{
    int start, end;     /* full range of codon, might be spliced */
    int start1, end1;   /* first part of codon */
    int iExon1;         /* exon containing first part */
    int start2, end2;   /* second part of codon if spliced, or 0,0 */
    int iExon2;         /* exon containing second part */
};
static struct codonCoords zeroCodonCoords = {0, 0, 0, 0, 0, 0, 0, 0};


static void writeCodon(FILE *f, char *source, char *name, char *geneName,
                       char *chrom, char strand, char *type, 
                       struct codonCoords *codon)
/* write the location of a codon to the GTF, if it is non-zero.  */
{
if (codon->start1 < codon->end1)
    {
    writeGtfLine(f, source, name, geneName, chrom, strand, type, 
                 codon->start1, codon->end1, codon->iExon1, 0);
        
    if (codon->start2 < codon->end2)
        writeGtfLine(f, source, name, geneName, chrom, strand, type, 
                     codon->start2, codon->end2, codon->iExon2, 
                     (codon->end1-codon->start1));
    }
}

static boolean inBounds(struct genePred *gp, int start, int end, int iExon)
/* test if a base range is in bounds of an exon or empty. */
{
return (start >= end)
    || ((gp->exonStarts[iExon] <= start) && (end <= gp->exonEnds[iExon]));
}

static struct codonCoords mkCodonCoords(struct genePred *gp, int iExon, int start)
/* Build a codonCoords struct starting at the specified location.  Make
 * sure bounds are in exon; might be out of bounds if gappy genePred */
{
struct codonCoords codon = zeroCodonCoords;
codon.start1 = start;
codon.end1 = codon.start1 + 3;
codon.iExon1 = iExon;
if (codon.end1 > gp->exonEnds[iExon])
    {
    if (iExon == gp->exonCount-1)
        return zeroCodonCoords;
    else
        {
        codon.end1 = gp->exonEnds[iExon];
        codon.start2 = gp->exonStarts[iExon+1];
        codon.end2 = codon.start2 + (3 - (codon.end1 - codon.start1));
        codon.iExon2 = iExon+1;
        }
    }
if (!(inBounds(gp, codon.start1, codon.end1, codon.iExon1)
      && inBounds(gp, codon.start2, codon.end2, codon.iExon2)))
    return zeroCodonCoords;
codon.start = codon.start1;
codon.end = (codon.start2 < codon.end2) ? codon.end2 : codon.end1;
return codon;
}

static void getStartStopCoords(struct genePred *gp, struct codonCoords *firstCodon,
                               struct codonCoords *lastCodon)
/* get the coordinates of the start and stop codons */
{
int i;
*firstCodon = zeroCodonCoords;
*lastCodon = zeroCodonCoords;

/* find the exons containing the first and last codons in gene */
for (i=0; i<gp->exonCount; ++i)
    {
    if ((gp->exonStarts[i] <= gp->cdsStart) && (gp->cdsStart < gp->exonEnds[i]))
        {
        /* exon contains CDS start */
        *firstCodon = mkCodonCoords(gp, i, gp->cdsStart);
        }
    if ((gp->exonStarts[i] < gp->cdsEnd) && (gp->cdsEnd <= gp->exonEnds[i]))
        {
        /* exon contains CDS end */
        int first = gp->cdsEnd-3;  // back up to start of codon
        if (first < gp->exonStarts[i])
            {
            if (i > 0)
                {
                int off = gp->exonStarts[i]-first;
                *lastCodon = mkCodonCoords(gp, i-1, gp->exonEnds[i-1]-off);
                }
            }
        else
            *lastCodon = mkCodonCoords(gp, i, first);
        }
    }

if (honorCdsStat && (gp->optFields & genePredCdsStatFld))
    {
    if (gp->cdsStartStat != cdsComplete)
        *firstCodon = zeroCodonCoords;
    if (gp->cdsEndStat != cdsComplete)
        *lastCodon = zeroCodonCoords;
    }
}

static int *calcFrames(struct genePred *gp)
/* compute frames for a genePred the doesn't have them.  Free resulting array */
{
int *frames = needMem(gp->exonCount*sizeof(int));
int iStart = (gp->strand[0] == '+') ? 0 : gp->exonCount - 1;
int iStop = (gp->strand[0] == '+') ? gp->exonCount : -1;
int iIncr = (gp->strand[0] == '+') ? 1 : -1;
int i, cdsStart, cdsEnd;
int cdsBaseCnt = 0;
for (i = iStart; i != iStop; i += iIncr)
    {
    if (genePredCdsExon(gp, i, &cdsStart, &cdsEnd))
        {
        frames[i] = cdsBaseCnt % 3;
        cdsBaseCnt += (cdsEnd - cdsStart);
        }
    else
        frames[i] = -1;
    }
return frames;
}

static void writeFeatures(struct genePred *gp, int i, char *source, char *name,
                          char *chrom, char strand, char *geneName,
                          int firstUtrEnd, int cdsStart, int cdsEnd, int lastUtrStart,
                          int frame, FILE *f)
/* write exons CDS/UTR features */
{
int exonStart = gp->exonStarts[i];
int exonEnd = gp->exonEnds[i];
if (utr && (exonStart < firstUtrEnd))
    {
    int end = min(exonEnd, firstUtrEnd);
    writeGtfLine(f, source, name, geneName, chrom, strand,
                 ((strand == '+') ? "5UTR" : "3UTR"),
                 exonStart, end, i, -1);
    }
if ((cdsStart < exonEnd) && (cdsEnd > exonStart))
        {
        int start = max(exonStart, cdsStart);
        int end = min(exonEnd, cdsEnd);
        writeGtfLine(f, source, name, geneName, chrom, strand, "CDS",
                     start, end, i, frame);
        }
if (utr && (exonEnd > lastUtrStart))
    {
    int start = max(lastUtrStart, exonStart);
    writeGtfLine(f, source, name, geneName, chrom, strand,
                 ((strand == '+') ? "3UTR" : "5UTR"),
                 start, exonEnd, i, -1);
    }
}

void genePredWriteToGtf(struct genePred *gp, char *source, 
	struct hash *dupeHash, FILE *f)
/* Write out genePredName to GTF file. */
{
int i;
char *name = findUniqueName(dupeHash, gp->name);
char *geneName = gp->name2;
char *chrom = gp->chrom;
char strand = gp->strand[0];
struct codonCoords firstCodon, lastCodon;
getStartStopCoords(gp, &firstCodon, &lastCodon);
int *frames = (gp->optFields & genePredExonFramesFld) ? gp->exonFrames : calcFrames(gp);

// figure out bounds of CDS and UTR regions
int firstUtrEnd = gp->cdsStart;
int cdsStart = (strand == '+') ? gp->cdsStart : firstCodon.end;
int cdsEnd = (strand == '+') ? lastCodon.start : gp->cdsEnd;
int lastUtrStart = gp->cdsEnd;

if (stopBetweenCdsUtr)
    {
    if (strand == '+')
        lastUtrStart = lastCodon.end;
    else
        firstUtrEnd = firstCodon.start;
    }

for (i=0; i<gp->exonCount; ++i)
    {
    writeGtfLine(f, source, name, geneName, chrom, strand, "exon", 
             gp->exonStarts[i], gp->exonEnds[i], i, -1);
    if (gp->cdsStart < gp->cdsEnd)
        writeFeatures(gp, i, source, name, chrom, strand, geneName, 
                      firstUtrEnd, cdsStart, cdsEnd, lastUtrStart,
                      frames[i], f);
    }
if (gp->strand[0] == '+')
    {
    writeCodon(f, source, name, geneName, chrom, strand, "start_codon", 
               &firstCodon);
    writeCodon(f, source, name, geneName, chrom, strand, "stop_codon",
               &lastCodon);
    }
else
    {
    writeCodon(f, source, name, geneName, chrom, strand, "start_codon",
               &lastCodon);
    writeCodon(f, source, name, geneName, chrom, strand, "stop_codon",
               &firstCodon);
    }
if (!(gp->optFields & genePredExonFramesFld))
    freeMem(frames);
}

void genePredToGtf(char *database, char *table, char *gtfOut)
/* genePredToGtf - Convert genePredName table or file to gtf.. */
{
FILE *f = mustOpen(gtfOut, "w");
struct hash *dupeHash = newHash(16);
struct genePred *gpList = NULL, *gp = NULL;

if (sameString(database, "file"))
    {
    gpList = genePredReaderLoadFile(table, NULL);
    }
else
    {
    struct sqlConnection *conn = sqlConnect(database);
    gpList = genePredReaderLoadQuery(conn, table, NULL);
    sqlDisconnect(&conn);
    }
slSort(&gpList, genePredCmp);

for (gp = gpList; gp != NULL; gp = gp->next)
    genePredWriteToGtf(gp, source, dupeHash, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
utr = optionExists("utr");
stopBetweenCdsUtr = optionExists("stopBetweenCdsUtr");
if (stopBetweenCdsUtr)
    utr = TRUE;
honorCdsStat = optionExists("honorCdsStat");
source = optionVal("source", argv[2]);
genePredToGtf(argv[1], argv[2], argv[3]);
return 0;
}
