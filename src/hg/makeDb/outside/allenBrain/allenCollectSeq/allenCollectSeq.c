/* allenCollectSeq - Collect probe sequences for Allen Brain Atlas from a variety of sources. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dnaseq.h"
#include "fa.h"
#include "dnaLoad.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "allenCollectSeq - Collect probe sequences for Allen Brain Atlas from a variety of sources\n"
  "usage:\n"
  "  allenCollectSeq tabFile probeFile nmFile xmFile tcFile extraFile outFile.fa refToRp.tab missing.tab rpToUrl.tab\n"
  "Where tabFile is a tab-separated file with the following fields\n"
  "\t#geneSymbol     geneName        entrezGeneId    refSeqAccessionNumber   urlToAtlas\n"
  "and probeFile is a fasta file containing header lines of the format\n"
  "\t>aibs|14182|sym|Gabrg2|entrez|14406|refseq|NM_008073|probe|RP_040227_01_01\n"
  "and nmFile is a simple fasta file of NCBI refSeqsequences (NM_ sequences)\n"
  "and xmFile is a simple fasta file of NCBI gene model sequences (XM_ sequences)\n"
  "and tcFile is a simple fasta file of TIGR MGI TC sequence\n"
  "and extraFile is a simple fasta file of other sequence\n"
  "The output files are\n"
  "  outFile.fa - a fasta file containing the combined probe sequences\n"
  "  refToRp.tab - this maps the refSeqAccessionNumber in tabFile to the outFile.fa ids\n"
  "  missing.tab - this contains lines from tabFile where no sequence could be found\n"
  "  rpToUrl.tab - this maps the outFile.fa ids to URLs for linking to Allen Brain Atlas\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *hashFa(char *fileName)
/* Read in fasta file and return hash of dnaSeq. */
{
struct dnaSeq *seq, *seqList;
struct hash *hash = hashNew(16);
seqList = dnaLoadAll(fileName);
for (seq = seqList; seq != NULL; seq = seq->next)
    hashAdd(hash, seq->name, seq);
verbose(1, "%d sequences in %s\n", slCount(seqList), fileName);
return hash;
}


struct hash *hashFaComponent(struct dnaSeq *seqList, char *before)
/* Given a list of sequence with names that are really |-separated
 * NCBI style things,  make a hash keyed by the name component
 * that occurs after the 'before' symbol */
{
struct dyString *dy = dyStringNew(0);
char *words[32];
int i,wordCount;
struct dnaSeq *seq;
struct hash *hash = hashNew(16);

for (seq = seqList; seq != NULL; seq = seq->next)
    {
    boolean gotIt = FALSE;
    dyStringClear(dy);
    dyStringAppend(dy, seq->name);
    wordCount = chopByChar(dy->string, '|', words, ArraySize(words));
    for (i=0; i<wordCount-1; i += 2)
        {
	if (sameString(words[i], before))
	    {
	    hashAdd(hash, words[i+1], seq);
	    gotIt = TRUE;
	    }
	}
    if (!gotIt)
	errAbort("No %s in %s", before, seq->name);
    }
dyStringFree(&dy);
return hash;
}

void simplifySeqName(struct dnaSeq *seqList, char *before)
/* Given a list of sequence with names that are really |-separated
 * NCBI style things,  convert name into just the component that
 * happens after 'before' symbol. */
{
struct dnaSeq *seq;
char *words[32];
int i,wordCount;
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    boolean gotIt = FALSE;
    wordCount = chopByChar(seq->name, '|', words, ArraySize(words));
    for (i=0; i<wordCount-1; i += 2)
        {
	if (sameString(words[i], before))
	    {
	    strcpy(seq->name, words[i+1]);
	    gotIt = TRUE;
	    }
	}
    if (!gotIt)
	errAbort("No %s in %s", before, seq->name);
    }
}

void allenCollectSeq(char *tabFile, char *probeFile, char *nmFile,
	char *xmFile, char *tcFile, char *extraFile,
	char *outFa, char *outRefToRp, char *outMiss, char *outRpToUrl)
/* allenCollectSeq - Collect probe sequences for Allen Brain Atlas from a variety of sources. */
{
struct lineFile *lf = lineFileOpen(tabFile, TRUE);
FILE *fFa = mustOpen(outFa, "w");
FILE *fRefToRp = mustOpen(outRefToRp, "w");
FILE *fRpToUrl = mustOpen(outRpToUrl, "w");
FILE *fMiss = mustOpen(outMiss, "w");
char *row[5];
struct hash *nmHash = hashFa(nmFile);
struct hash *xmHash = hashFa(xmFile);
struct hash *tcHash = hashFa(tcFile);
struct hash *extraHash = hashFa(extraFile);
struct dnaSeq *probeList = dnaLoadAll(probeFile);
struct hash *probeHash = hashFaComponent(probeList, "refseq");
int hitProbe = 0, hitXm = 0, hitNm = 0, hitTc = 0, hitNone = 0, hitExtra = 0, hitTotal = 0;

verbose(1, "%d sequences in %s\n", slCount(probeList), probeFile);
simplifySeqName(probeList, "probe");

while (lineFileRowTab(lf, row))
    {
    char *acc = row[3];
    char *accChop = NULL;
    char *s = strrchr(acc,'.');
    char *acc0 = addSuffix(acc,".0");
    char *acc1 = addSuffix(acc,".1");
    char *acc2 = addSuffix(acc,".2");
    char *acc3 = addSuffix(acc,".3");
    char *acc4 = addSuffix(acc,".4");
    if (s && (strchr("0123456789",s[1])) && s[2]==0)  /* ends in [.][0-9] */
	{
    	accChop = cloneStringZ(acc,s-acc);
	}
    struct dnaSeq *seq = NULL;
    if ((seq = hashFindVal(probeHash, acc)) != NULL)
	++hitProbe;
    else if (accChop && ((seq = hashFindVal(probeHash, accChop)) != NULL))
	++hitProbe;
    else if ((seq = hashFindVal(probeHash, acc0)) != NULL)
	++hitProbe;
    else if ((seq = hashFindVal(probeHash, acc1)) != NULL)
	++hitProbe;
    else if ((seq = hashFindVal(probeHash, acc2)) != NULL)
	++hitProbe;
    else if ((seq = hashFindVal(probeHash, acc3)) != NULL)
	++hitProbe;
    else if ((seq = hashFindVal(probeHash, acc4)) != NULL)
	++hitProbe;
    else if ((seq = hashFindVal(nmHash, acc)) != NULL)
	++hitNm;
    else if ((seq = hashFindVal(xmHash, acc)) != NULL)
	++hitXm;
    else if ((seq = hashFindVal(tcHash, acc)) != NULL)
        ++hitTc;
    else if ((seq = hashFindVal(extraHash, acc)) != NULL)
        ++hitExtra;
    else
	{
	int i;
	verbose(2, "Can't find probe for %s\n", acc);
	for (i=0; i<ArraySize(row); ++i)
	    fprintf(fMiss, "%s\t", row[i]);
	fprintf(fMiss, "\n");

        ++hitNone;
	}
    if (seq != NULL)
        {
	char seqName[128];
	if (startsWith("RP_", seq->name))
	    safef(seqName, sizeof(seqName), "%s", seq->name);
	else
	    safef(seqName, sizeof(seqName), "RP_%s", seq->name);
	fprintf(fRefToRp, "%s\t%s\n", acc, seqName);
	fprintf(fRpToUrl, "%s\t%s\n", seqName, row[4]);
	if (row[4][0] == 0)
	    warn("Missing url line %d of %s\n", lf->lineIx, lf->fileName);

	faWriteNext(fFa, seqName, seq->dna, seq->size);
	}
    ++hitTotal;
    freeMem(accChop);
    freeMem(acc0);
    freeMem(acc1);
    freeMem(acc2);
    freeMem(acc3);
    freeMem(acc4);
    }
verbose(1, "%d (%3.1f%%) hitProbe\n", hitProbe, 100.0 * hitProbe/hitTotal);
verbose(1, "%d (%3.1f%%) hitNm\n", hitNm, 100.0 * hitNm/hitTotal);
verbose(1, "%d (%3.1f%%) hitXm\n", hitXm, 100.0 * hitXm/hitTotal);
verbose(1, "%d (%3.1f%%) hitTc\n", hitTc, 100.0 * hitTc/hitTotal);
verbose(1, "%d (%3.1f%%) hitExtra\n", hitExtra, 100.0 * hitExtra/hitTotal);
verbose(1, "%d (%3.1f%%) hitNone\n", hitNone, 100.0 * hitNone/hitTotal);
carefulClose(&fFa);
carefulClose(&fRpToUrl);
carefulClose(&fRefToRp);
carefulClose(&fMiss);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 11)
    usage();
allenCollectSeq(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7],
	argv[8], argv[9], argv[10]);
return 0;
}
