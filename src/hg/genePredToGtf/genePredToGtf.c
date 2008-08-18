/* genePredToGtf - Convert genePred table or file to gtf.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "genePred.h"
#include "genePredReader.h"

static char const rcsid[] = "$Id: genePredToGtf.c,v 1.15 2008/08/18 20:51:54 markd Exp $";

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
  "   -honorCdsStat - use cdsStartStat/cdsEndStat when defining start/end\n"
  "    codon records\n"
  "   -source=src set source name to uses\n"
  "   -addComments - Add comments before each set of transcript records.\n"
  "    allows for easier visual inspection\n"
  "Note: use refFlat or extended genePred table to include geneName\n"
  );
}

static struct optionSpec options[] = {
   {"utr", OPTION_BOOLEAN},
   {"honorCdsStat", OPTION_BOOLEAN},
   {"source", OPTION_STRING},
   {"addComments", OPTION_BOOLEAN},
   {NULL, 0}
};
static boolean utr = FALSE;
static boolean honorCdsStat = FALSE;
static char *source = NULL;
static boolean addComments = FALSE;

static int frameIncr(int frame, int amt)
/* increment frame by amt */
{
if (frame == -1)
    return -1;  // no frame
else
    return (frame+amt) % 3;
}

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
fprintf(f, "gene_id \"%s\"; ", (isNotEmpty(geneName)) ? geneName : name);
fprintf(f, "transcript_id \"%s\"; ", name);
fprintf(f, "exon_number \"%d\"; ", exonIx+1);
fprintf(f, "exon_id \"%s.%d\";", name, exonIx+1);
if (isNotEmpty(geneName))
    fprintf(f, " gene_name \"%s\";", geneName);
fprintf(f, "\n");
}

static boolean inExon(struct genePred *gp, int iExon, int pos)
/* determine if pos is in the specified exon */
{
return ((gp->exonStarts[iExon] <= pos) && (pos <= gp->exonEnds[iExon]));
}

static int movePos(struct genePred *gp, int pos, int dist)
/* Move a position in an exon by dist, which is positive to move forward, and
 * negative to move backwards. Introns are skipped.  Error if can't move
 * distance and stay in exon.
 */
{
int origPos = pos;
assert((gp->txStart <= pos) && (pos <= gp->txEnd));
// find containing exon
int iExon;
for (iExon = 0; iExon < gp->exonCount; iExon++)
    {
    if (inExon(gp, iExon, pos))
        break;
    }
if (iExon > gp->exonCount)
    errAbort("can't find %d in an exon of %s %s:%d-%d",
             pos, gp->name, gp->chrom, gp->txStart, gp->txEnd);

// adjust by distance
int left = intAbs(dist);
int dir = (dist >= 0) ? 1 : -1;
while ((0 <= iExon) && (iExon < gp->exonCount) && (left > 0))
    {
    if (inExon(gp, iExon, pos+dir))
        pos += dir;
    else if (dir >= 0)
        {
        // move to next exon
        iExon++;
        if (iExon < gp->exonCount)
            pos = gp->exonStarts[iExon];
        }
    else
        {
        // move to previous
        iExon--;
        if (iExon >= 0)
            pos = (gp->exonStarts[iExon]+gp->exonEnds[iExon])-1;
        }
    left--;
    }
if (left > 0)
    errAbort("can't move %d by %d and be an exon of %s %s:%d-%d",
             origPos, dist, gp->name, gp->chrom, gp->txStart, gp->txEnd);
return pos;
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

static boolean codonComplete(struct codonCoords *codon)
/* are all bases of codon defined? */
{
return (((codon->end1-codon->start1)+(codon->end2-codon->start2)) == 3);
}

static void writeCodon(FILE *f, char *source, char *name, char *geneName,
                       char *chrom, char strand, char *type, 
                       struct codonCoords *codon)
/* write the location of a codon to the GTF, if it is non-zero and complete */
{
writeGtfLine(f, source, name, geneName, chrom, strand, type, 
             codon->start1, codon->end1, codon->iExon1, 0);
        
if (codon->start2 < codon->end2)
    writeGtfLine(f, source, name, geneName, chrom, strand, type, 
                 codon->start2, codon->end2, codon->iExon2, 
                 (codon->end1-codon->start1));
}

static struct codonCoords findFirstCodon(struct genePred *gp, int *frames)
/* get the coordinates of the first codon (start or stop). It must be in
 * correct frame, or zero is returned. */
{
if (honorCdsStat && (gp->optFields & genePredCdsStatFld)
    && (gp->cdsStartStat != cdsComplete))
    return zeroCodonCoords;  // not flagged as complete

// find first CDS exon
int iExon, cdsStart = 0, cdsEnd = 0;
for (iExon = 0; iExon < gp->exonCount; iExon++)
    {
    if (genePredCdsExon(gp, iExon, &cdsStart, &cdsEnd))
        break;
    }
if (iExon == gp->exonCount)
    return zeroCodonCoords;  // no CDS

// get frame and validate that we are on a bound.
int frame = (gp->strand[0] == '+') ? frames[iExon]
    : frameIncr(frames[iExon], (cdsEnd-cdsStart));
if (frame != 0)
    return zeroCodonCoords;  // not on a frame boundary

/* get first part of codon */
struct codonCoords codon = zeroCodonCoords;
codon.start= codon.start1 = cdsStart;
codon.end = codon.end1 = codon.start1 + min(cdsEnd-cdsStart, 3);

/* second part, if spliced */
if ((codon.end1 - codon.start1) < 3)
    {
    iExon++;
    if ((iExon == gp->exonCount) || !genePredCdsExon(gp, iExon, &cdsStart, &cdsEnd))
        return zeroCodonCoords;  // no more
    int needed = 3 - (codon.end1 - codon.start1);
    if ((cdsEnd - cdsStart) < needed)
        return zeroCodonCoords;  // not enough space
    codon.start2 = cdsStart;
    codon.end = codon.end2 = codon.start2 + needed;
    }
return codon;
}

static struct codonCoords findLastCodon(struct genePred *gp, int *frames)
/* get the coordinates of the last codon (start or stop). It must be in
 * correct frame, or zero is returned. */
{
if (honorCdsStat && (gp->optFields & genePredCdsStatFld)
    && (gp->cdsEndStat != cdsComplete))
    return zeroCodonCoords;  // not flagged as complete

// find last CDS exon
int iExon, cdsStart = 0, cdsEnd = 0;
for (iExon = gp->exonCount; iExon >= 0; iExon--)
    {
    if (genePredCdsExon(gp, iExon, &cdsStart, &cdsEnd))
        break;
    }
if (iExon == -1)
    return zeroCodonCoords;  // no CDS

// get frame of last base and validate that we are on a bound.
int frame = (gp->strand[0] == '-') ? frames[iExon]
    : frameIncr(frames[iExon], (cdsEnd-cdsStart));
if (frame != 0)
    return zeroCodonCoords;  // not on a frame boundary

/* get last part of codon */
struct codonCoords codon = zeroCodonCoords;
codon.start= codon.start1 = max(cdsStart, cdsEnd-3);
codon.end = codon.end1 = cdsEnd;

/* first part, if spliced */
if ((codon.end1 - codon.start1) < 3)
    {
    codon.start2 = codon.start1;
    codon.end = codon.end2 = codon.end1;
    iExon--;
    if ((iExon == -1) || !genePredCdsExon(gp, iExon, &cdsStart, &cdsEnd))
        return zeroCodonCoords;  // no more
    int needed = 3 - (codon.end2 - codon.start2);
    if ((cdsEnd - cdsStart) < needed)
        return zeroCodonCoords;  // not enough space
    codon.start = codon.start1 = cdsEnd-needed;
    codon.end1 = cdsEnd;
    }
return codon;
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
int *frames = (gp->optFields & genePredExonFramesFld) ? gp->exonFrames : calcFrames(gp);
struct codonCoords firstCodon = findFirstCodon(gp, frames);
struct codonCoords lastCodon = findLastCodon(gp, frames);

// figure out bounds of CDS and UTR regions, moving stop codon to outside of
// CDS.
int firstUtrEnd = gp->cdsStart, lastUtrStart = gp->cdsEnd;
int cdsStart = gp->cdsStart, cdsEnd = gp->cdsEnd;
if ((strand == '+') && codonComplete(&lastCodon))
    cdsEnd = movePos(gp, lastUtrStart, -3);
if ((strand == '-') && codonComplete(&firstCodon))
    cdsStart = movePos(gp, cdsStart, 3);

if (addComments)
    fprintf(f, "###\n# %s %s:%d-%d (%s) CDS: %d-%d\n#\n",
            gp->name, gp->chrom, gp->txStart, gp->txEnd,
            gp->strand, gp->cdsStart, gp->cdsEnd);

for (i=0; i<gp->exonCount; ++i)
    {
    writeGtfLine(f, source, name, geneName, chrom, strand, "exon", 
             gp->exonStarts[i], gp->exonEnds[i], i, -1);
    if (cdsStart < cdsEnd)
        writeFeatures(gp, i, source, name, chrom, strand, geneName, 
                      firstUtrEnd, cdsStart, cdsEnd, lastUtrStart,
                      frames[i], f);
    }
if (gp->strand[0] == '+')
    {
    if (codonComplete(&firstCodon))
        writeCodon(f, source, name, geneName, chrom, strand, "start_codon", 
                   &firstCodon);
    if (codonComplete(&lastCodon))
        writeCodon(f, source, name, geneName, chrom, strand, "stop_codon",
                   &lastCodon);
    }
else
    {
    if (codonComplete(&firstCodon))
        writeCodon(f, source, name, geneName, chrom, strand, "start_codon",
                   &lastCodon);
    if (codonComplete(&lastCodon))
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
honorCdsStat = optionExists("honorCdsStat");
source = optionVal("source", argv[2]);
addComments = optionExists("addComments");
genePredToGtf(argv[1], argv[2], argv[3]);
return 0;
}
