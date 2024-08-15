/* bamToPsl - Convert a bam file to a psl and optionally also a fasta file that contains the reads.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"
#include "psl.h"
#include "fa.h"
#include "net.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bamToPsl - Convert a bam file to a psl and optionally also a fasta file that contains the reads.\n"
  "usage:\n"
  "   bamToPsl [options] in.bam out.psl\n"
  "options:\n"
  "   -fasta=output.fa - output query sequences to specified file\n"
  "   -chromAlias=file - specify a two-column file: 1: alias, 2: other name\n"
  "          for target name translation from column 1 name to column 2 name\n"
  "          names not found are passed through intact\n"
  "   -nohead          - do not output the PSL header, default has header output\n"
  "   -dots=N          - output progress dot(.) every N alignments processed\n"
  "\n"
  "Note: a chromAlias file can be obtained from a UCSC database, e.g.:\n"
  " hgsql -N -e 'select alias,chrom from chromAlias;' hg38 > hg38.chromAlias.tab\n"
  " Or from the downloads server:\n"
  "  wget https://hgdownload.soe.ucsc.edu/goldenPath/hg38/database/chromAlias.txt.gz\n"
  "See also our tool chromToUcsc\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"fasta", OPTION_STRING},
   {"chromAlias", OPTION_STRING},
   {"nohead", OPTION_BOOLEAN},
   {"allowDups", OPTION_BOOLEAN},
   {"noSequenceVerify", OPTION_BOOLEAN},
   {"dots", OPTION_INT},
   {"querySizes", OPTION_STRING},
   {NULL, 0},
};

static int dots = 0;
static boolean nohead = FALSE;

static struct hash *hashChromAlias(char *fileName)
/* Read two column file into hash keyed by first column */
{
struct hash *hash = hashNew(0);
struct lineFile *lf = netLineFileOpen(fileName);
char *row[2];
while (lineFileRow(lf, row))
    hashAdd(hash, row[0], cloneString(row[1]));

lineFileClose(&lf);
return hash;
}

static void convertToPsl(bam1_t *aln, bam_header_t *head, struct hash *chromAlias, FILE *f)
/* convert one alignment to a psl */
{
// includes hard masked in PSL so that supplementary alignments create PSLs that reflect the
// entire query
struct psl *psl = bamToPslUnscored2(aln, head, TRUE);
if (psl != NULL)
    {
    if (chromAlias)
        {
        struct hashEl *hel = NULL;
        if ((hel = hashLookup(chromAlias, psl->tName)) != NULL)
            psl->tName = cloneString((char *)hel->val); /* memory leak */
        }
    pslTabOut(psl, f);  /* no free of this psl data, memory leak */
    pslFree(&psl);
    }
}

static void convertToFasta(bam1_t *aln, struct hash *fastaDoneSeqs, FILE *faF)
/* output a FASTA record of the query sequence */
{
char *dna = bamGetQuerySequence(aln, TRUE);
char *qName = bam1_qname(aln);
if (hashLookup(fastaDoneSeqs, qName) == NULL) // first seen
    {
    hashAddInt(fastaDoneSeqs, qName, TRUE);
    faWriteNext(faF, qName, dna, strlen(dna));
    }
freez(&dna);
}

static void bamToPsl(char *inBam, char *outPsl, char *outFasta, char *aliasFile)
/* bamToPsl - Convert a bam file to a psl and optionally also a
   fasta file that contains the reads.. */
{
unsigned long long processCount = 0;
samfile_t *in = bamMustOpenLocal(inBam, "rb", NULL);
bam_header_t *head = sam_hdr_read(in);
if (head == NULL)
    errAbort("Aborting ... bad BAM header in %s", inBam);

/* Open up psl output and write header (if allowed). */
FILE *f = mustOpen(outPsl, "w");
if (! nohead)
    pslWriteHead(f);

/* Optionally use alias file */
struct hash *chromAlias = NULL;  /* initialize later if needed */
if (aliasFile != NULL)
    chromAlias = hashChromAlias(aliasFile);

/* Optionally open up fasta output */
struct hash *fastaDoneSeqs = NULL; // avoids duplicates
FILE *faF = NULL;
if (outFasta != NULL)
    {
    faF = mustOpen(outFasta, "w");
    fastaDoneSeqs = hashNew(20);
    }

bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
/* Write next sequence to fa file. */
for (;;)
    {
    if (sam_read1(in, head, &one) < 0)
	{
	break;
	}
    if (one.core.n_cigar != 0)
        {
        convertToPsl(&one, head, chromAlias, f);
        }
    if ((faF != NULL) && ((one.core.flag & BAM_FSUPPLEMENTARY) == 0))
        {
        // supplementary are not include as they don't have full sequence
        convertToFasta(&one, fastaDoneSeqs, faF);
        }
    ++processCount;
    if (dots)
       if (0 == processCount % dots)
          verbose(1,".");
    }
    if (dots)
       verbose(1,"\n");

samclose(in);
carefulClose(&f);
carefulClose(&faF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *fastaName = optionVal("fasta", NULL);
char *aliasFile = optionVal("chromAlias", NULL);

dots = optionInt("dots", dots);
nohead = optionExists("nohead");
if (optionExists("allowDups"))
    fprintf(stderr, "Note: -allowDups is obsolete and ignored");
if (optionExists("noSequenceVerify"))
    fprintf(stderr, "Note: -noSequenceVerify is obsolete and ignored");
if (optionExists("querySizes"))
    fprintf(stderr, "Note: -querySizes is obsolete and ignored");
bamToPsl(argv[1], argv[2], fastaName, aliasFile);
return 0;
}
