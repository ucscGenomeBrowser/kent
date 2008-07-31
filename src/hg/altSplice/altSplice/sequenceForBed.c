/* Sequence for bed. Takes a file of beds and
   ouputs the sequence for them to a fasta file. */
#include "common.h"
#include "hdb.h"
#include "fa.h"
#include "dnaseq.h"
#include "obscure.h"
#include "bed.h"
#include "options.h"


static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"db", OPTION_STRING},       /* Database to use. */
    {"fastaOut", OPTION_STRING}, /* File to print fasta records to. */
    {"bedIn", OPTION_STRING},    /* File to read bed records from. */
    {"upCase", OPTION_BOOLEAN},  /* Upcase the file. */
    {"keepName", OPTION_BOOLEAN}, /* Keep id's in fa file same as bed name. */
    {NULL, 0}
};

boolean upCase = FALSE;
boolean keepName = FALSE;

void usage()
{
errAbort("sequenceForBed - Writes sequence for beds to a fasta\n"
	 "file. Requires database access.\n"
	 "usage:\n   "
	 "sequenceForBed -db=hg15 -bedIn=someFile.bed -fastaOut=fileWithSeq.fa\n"
	 "\n"
	 "optional args:\n"
	 "   -upCase: output sequence as uppercase\n"
	 "   -keepName: keep id's in fa file same as bed name (no chromosome stuff)\n"
	 );
}

void sequenceForBed(char *db, char *bedFile, char *fastaFile)
/* For each bed get fasta file and ouput. */
{
struct bed *bedList = NULL, *bed = NULL;
struct dnaSeq *seq = NULL;
FILE *fasta = NULL;
char nameBuff[2056];
assert(db);
assert(bedFile);
assert(fastaFile);

warn("Reading beds.");
bedList = bedLoadAll(bedFile);
dotForUserInit(slCount(bedList)/20 + 1);

fasta = mustOpen(fastaFile, "w");
warn("Writing sequences.");
for(bed = bedList; bed != NULL; bed = bed->next) 
    {
    dotForUser();
    seq = hSeqForBed(db, bed);
    if (keepName)
	safef(nameBuff, sizeof(nameBuff), "%s %s:%d-%d %s strand", bed->name, 
	      bed->chrom, bed->chromStart, bed->chromEnd, bed->strand);
    else
	safef(nameBuff, sizeof(nameBuff), "%s.%s.%s:%d-%d", bed->name, bed->strand, 
	      bed->chrom, bed->chromStart, bed->chromEnd);
    if (upCase)
	touppers(seq->dna);
    faWriteNext(fasta, nameBuff, seq->dna, seq->size);
    dnaSeqFree(&seq);
    }
warn("\nDone.\n");
}

int main(int argc, char *argv[])
/* Everybodys favorite function. */
{
char *db = NULL;
char *bedFile = NULL;
char *fastaFile = NULL;
if(argc <= 3)
    usage();
optionInit(&argc, argv, optionSpecs);
db = optionVal("db", NULL);
bedFile = optionVal("bedIn", NULL);
fastaFile = optionVal("fastaOut", NULL);
upCase = optionExists("upcase");
keepName = optionExists("keepName");

if(db == NULL || bedFile == NULL || fastaFile == NULL)
    {
    warn("Not enough arguments.");
    usage();
    }
sequenceForBed(db, bedFile, fastaFile);
return 0;
}

