/* bamToPsl - Convert a bam file to a psl and optionally also a fasta file that contains the reads.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"
#include "psl.h"
#include "fa.h"
#include "md5.h"
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
  "   -allowDups       - for fasta output, allow duplicate query sequences output\n"
  "                    - default is to eliminate duplicate sequences\n"
  "                    - runs much faster without the duplicate check\n"
  "   -noSequenceVerify - when checking for dups, do not verify each sequence\n"
  "                    - when the same name is identical, assume they are\n"
  "                    - helps speed up the dup check but not thorough\n"
  "   -dots=N          - output progress dot(.) every N alignments processed\n"
  "\n"
  "note: a chromAlias file can be obtained from a UCSC database, e.g.:\n"
  " hgsql -N -e 'select alias,chrom from chromAlias;' hg38 > hg38.chromAlias.tab"
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
   {NULL, 0},
};

static int dots = 0;
static boolean nohead = FALSE;
static boolean allowDups = FALSE;
static boolean noSequenceVerify = FALSE;

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

static void bamToPsl(char *inBam, char *outPsl, char *outFasta, char *aliasFile)
/* bamToPsl - Convert a bam file to a psl and optionally also a
   fasta file that contains the reads.. */
{
unsigned long long processCount = 0;
       /* record md5sums to avoid duplicate fasta output
          key is name, value is md5sum */
struct hash *fastaSums = NULL;  /* initialize later if needed */
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
FILE *faF = NULL;
if (outFasta != NULL)
    {
    faF = mustOpen(outFasta, "w");
    fastaSums = newHashExt(20, TRUE);  /* using stack local memory */
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
        struct psl *psl = bamToPslUnscored(&one, head);
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
    ++processCount;
    if (dots)
       if (0 == processCount % dots)
          verbose(1,".");
    if (faF != NULL)
        {
	char *dna = bamGetQuerySequence(&one, TRUE);
	char *qName = bam1_qname(&one);
        if (allowDups)
	    faWriteNext(faF, qName, dna, strlen(dna));
        else
            {
            struct hashEl *hel = NULL;
            if ((hel = hashLookup(fastaSums, qName)) == NULL) // first seen
               {
               char *md5sum = md5HexForString(dna);
               hel = hashAdd(fastaSums, qName, md5sum);
               faWriteNext(faF, qName, dna, strlen(dna));
               }
            else if (! noSequenceVerify)  // repeated md5sum calculation
               {
               char *md5sum = md5HexForString(dna);
               /* verify sequence is identical for same name */
               if (differentWord((char *)hel->val, md5sum))
                  verbose(1, "# warning: different sequence found for '%s'\n",
                             qName);
               }
            }
	freez(&dna);
	}
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
allowDups = optionExists("allowDups");
noSequenceVerify = optionExists("noSequenceVerify");
bamToPsl(argv[1], argv[2], fastaName, aliasFile);
return 0;
}
