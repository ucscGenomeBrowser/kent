/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* scaffoldFaToAgp - read a file containing FA scaffold records and generate */
/*                   an AGP file, gap file and a lift file */
/* This utility is used when working with genomes that consist
 * of scaffolds with no chromosome mapping (e.g. Fugu).
 * The AGP file contains scaffolds and inter-scaffold gaps.
 * This file is used to generate the assembly ("gold") track.
 * The "gap" file contains gaps within scaffolds as well
 * as inter-scaffold gaps.  This file is used to generate the "gap" track.
 * It is formatted similarly to an AGP, but only contains gap entries.
 * The lift file is associated with the AGP file.  It is used to
 * lift scaffold coordinates to chromosome coordinates.
 */

#include "common.h"
#include "fa.h"
#include "options.h"
#include "../../hg/inc/agpFrag.h"
#include "../../hg/inc/agpGap.h"


#define SCAFFOLD_GAP_TYPE "contig"   
#define FRAGMENT_GAP_TYPE "frag"        /* within scaffolds (bridged) */
#define CHROM_NAME "chrUn"

int minGapSize = 5;
int scaffoldGapSize = 1000;

void usage()
/* Print usage instructions and exit. */
{
errAbort("scaffoldFaToAgp - generate an AGP file, gap file, and lift file from a scaffold FA file.\n"
     "usage:\n"
     "    scaffoldFaToAgp source.fa\n"
     "options:\n"
     "      -minGapSize   Minimum threshold for calling a block of Ns a gap.\n"
     "      -scaffoldGapSize Size of gaps of Ns to be inserted between scaffolds.\n"
     "The resulting files will be source.{agp,gap,lft}\n"
     "Note: unless otherwise specified, gaps of 1000 bases are inserted \n"
     "between scaffold records by default as contig gaps in the .agp file.\n"
     "  N's within scaffolds are represented as\n"
     "   frag gaps in the .gap file only\n");
}

boolean seqGetGap(DNA *seq, int *retSize, int *retGapSize)
/* determine size of ungapped sequence, and size of gap following it */
{
int seqSize = 0, gapSize = 0;

for (; *seq && (*seq != 'n' && *seq != 'N'); seq++, seqSize++);
if (retSize)
    *retSize = seqSize;
for (; *seq && (*seq == 'n' || *seq == 'N'); seq++, gapSize++);
if (retGapSize)
    *retGapSize = gapSize;
return (seqSize ? TRUE : FALSE);
}

void scaffoldFaToAgp(char *scaffoldFile)
/* scaffoldFaToAgp - create AGP file, gap file and lift file 
* from scaffold FA file */
{
DNA *scaffoldSeq;
char *name;
int size;
struct agpFrag frag;
struct agpGap scaffoldGap, fragGap;

struct lineFile *lf = lineFileOpen(scaffoldFile, TRUE);
char outDir[256], outFile[128], ext[64], outPath[512];
FILE *agpFile = NULL, *gapFile = NULL, *liftFile = NULL;

int fileNumber = 1;      /* sequence number in AGP file */
int start = 0, end = 0;
int chromSize = 0;
int scaffoldCount = 0;

int fragSize = 0, gapSize = 0;
char *seq;
int seqStart = 0;

/* determine size of "unordered chromosome" that will be constructed.
 * This is needed for the lift file. */
while (faMixedSpeedReadNext(lf, &scaffoldSeq, &size, &name))
    {
    chromSize += size;
    chromSize += scaffoldGapSize;
    scaffoldCount++;
    }
/* do not need the final useless gap */
chromSize -= scaffoldGapSize;
printf("scaffold gap size is %d, total scaffolds: %d\n",
         scaffoldGapSize, scaffoldCount);
printf("chrom size is %d\n", chromSize);

/* initialize fixed fields in AGP frag */
ZeroVar(&frag);
frag.chrom = CHROM_NAME;
frag.type[0] = 'D';   /* draft */
frag.fragStart = 0;   /* always start at beginning of scaffold */
frag.strand[0] = '+';

/* initialize fixed fields in scaffold gap */
ZeroVar(&scaffoldGap);
scaffoldGap.chrom = CHROM_NAME;
scaffoldGap.n[0] = 'N';
scaffoldGap.size = scaffoldGapSize;
scaffoldGap.type = SCAFFOLD_GAP_TYPE;
scaffoldGap.bridge = "no";

/* initialize fixed fields in frag gap */
ZeroVar(&fragGap);
fragGap.chrom = CHROM_NAME;
fragGap.n[0] = 'N';
fragGap.type = FRAGMENT_GAP_TYPE;
fragGap.bridge = "yes";

/* munge file paths */
splitPath(scaffoldFile, outDir, outFile, ext);

sprintf(outPath, "%s%s.agp", outDir, outFile);
agpFile = mustOpen(outPath, "w");
printf("writing %s\n", outPath);

sprintf(outPath, "%s%s.gap", outDir, outFile);
gapFile = mustOpen(outPath, "w");
printf("writing %s\n", outPath);

sprintf(outPath, "%s%s.lft", outDir, outFile);
liftFile = mustOpen(outPath, "w");
printf("writing %s\n", outPath);

/* read in scaffolds from fasta file, and generate
 * the three files */
lineFileSeek(lf, 0, SEEK_SET);
boolean allDone = FALSE;
allDone = ! faMixedSpeedReadNext(lf, &scaffoldSeq, &size, &name);
while (! allDone)
    {
    end = start + size;

    /* setup AGP frag for the scaffold and write to AGP file */
    frag.frag = name;
    frag.ix = fileNumber++;
    frag.chromStart = start;
    frag.chromEnd = end;
    frag.fragEnd = size;
    agpFragOutput(&frag, agpFile, '\t', '\n');

    /* write lift file entry for this scaffold */
    fprintf(liftFile, "%d\t%s\t%d\t%s\t%d\n",
            start, name, size, CHROM_NAME, chromSize);

    /* write gap file entries for this scaffold */
    seq = scaffoldSeq;
    seqStart = start;
    while (seqGetGap(seq, &fragSize, &gapSize))
        {
        if (gapSize > minGapSize)
            {
            fragGap.size = gapSize;
            fragGap.chromStart = seqStart + fragSize + 1;
            fragGap.chromEnd = fragGap.chromStart + gapSize - 1;
            agpGapOutput(&fragGap, gapFile, '\t', '\n');
            }
        seqStart = seqStart + fragSize + gapSize;
        seq = seq + fragSize + gapSize;
        }

    /* setup AGP gap to separate scaffolds and write to AGP and gap files */
    /* Note: may want to suppress final gap -- not needed as separator */
    start = end + 1;
    end = start + scaffoldGapSize - 1;

    /*	Avoid an extra gap on the end - not needed */
    allDone = ! faMixedSpeedReadNext(lf, &scaffoldSeq, &size, &name);
    if (allDone)
	break;

    scaffoldGap.ix = fileNumber++;
    scaffoldGap.chromStart = start;
    scaffoldGap.chromEnd = end;
    agpGapOutput(&scaffoldGap, agpFile, '\t', '\n');
    agpGapOutput(&scaffoldGap, gapFile, '\t', '\n');

    /* write lift file entry for this gap */
    fprintf(liftFile, "%d\t%s\t%d\t%s\t%d\n",
            start-1, "gap", scaffoldGapSize, CHROM_NAME, chromSize);

    start = end;

    //freeMem(seq);
    }
carefulClose(&agpFile);
carefulClose(&liftFile);
carefulClose(&gapFile);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
minGapSize = optionInt("minGapSize", minGapSize);
scaffoldGapSize = optionInt("scaffoldGapSize", scaffoldGapSize);

if (argc != 2)
    usage();
scaffoldFaToAgp(argv[1]);
return 0;
}
