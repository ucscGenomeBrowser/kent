/* encode2Manifest - Create a encode3 manifest file for encode2 files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "intValTree.h"
#include "options.h"
#include "encode/encodeExp.h"
#include "mdb.h"

char *expDb = "hgFixed";
char *metaDbs[] = {"hg19", "mm9"};
char *expTable = "encodeExp";
char *metaTable = "metaDb";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2Manifest - Create a encode3 manifest file for encode2 files\n"
  "usage:\n"
  "   encode2Manifest manifest.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct rbTree *makeExpContainer()
/* Create rbTree container of expression objects keyed by ix */
{
struct rbTree *container = intValTreeNew();
struct sqlConnection *expDbConn = sqlConnect(expDb);
char query[256];
safef(query, sizeof(query), "select * from %s", expTable);
struct sqlResult *sr = sqlGetResult(expDbConn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* Read in database structure. */
    struct encodeExp *ee = encodeExpLoad(row);
    intValTreeAdd(container, ee->ix, ee);
    }
sqlFreeResult(&sr);
sqlDisconnect(&expDbConn);
return container;
}

struct mdbObj *getMdbList(char *database)
/* Get list of metaDb objects for a database. */
{
struct sqlConnection *conn = sqlConnect(database);
struct mdbObj *list = mdbObjsQueryAll(conn, metaTable);
sqlDisconnect(&conn);
return list;
}

int unknownFormatCount = 0;

boolean isHistoneModAntibody(char *antibody)
/* Returns TRUE if it looks something like H3K4Me3 */
{
return antibody[0] == 'H' && isdigit(antibody[1]);
}

char *guessEnrichedIn(char *composite, char *dataType, char *antibody)
/* Figure out what it should be enriched in by looking at composite and dataType and antibody */
{
if (composite == NULL || dataType == NULL)
    errAbort("Got composite=%s, dataType=%s, can't guess", composite, dataType);
char *guess = "unknown";
if (sameString(dataType, "AffyExonArray")) guess = "exon";
else if (sameString(dataType, "Cage")) guess = "exon";
else if (sameString(dataType, "ChipSeq")) guess = "regulatory";
else if (sameString(dataType, "DnaseDgf")) guess = "promoter";
else if (sameString(dataType, "DnaseSeq")) guess = "promoter";
else if (sameString(dataType, "FaireSeq")) guess = "regulatory";
else if (sameString(dataType, "Gencode")) guess = "exon";
else if (sameString(dataType, "MethylArray")) guess = "promoter";
else if (sameString(dataType, "MethylRrbs")) guess = "promoter";
else if (sameString(dataType, "MethylSeq")) guess = "promoter";
else if (sameString(dataType, "Proteogenomics")) guess = "coding";
else if (sameString(dataType, "RipGeneSt")) guess = "exon";
else if (sameString(dataType, "RipSeq")) guess = "exon";
else if (sameString(dataType, "RipTiling")) guess = "exon";
else if (sameString(dataType, "RnaChip")) guess = "exon";
else if (sameString(dataType, "RnaPet")) guess = "exon";
else if (sameString(dataType, "RnaSeq")) guess = "exon";
else if (sameString(dataType, "Switchtear")) guess = "regulatory";
else if (sameString(dataType, "TfbsValid")) guess = "regulatory";

/* Do some fine tuning within Chip-seq to treat histone mods differently. */
if (sameString(dataType, "ChipSeq"))
    {
    if (antibody == NULL)
       errAbort("Missing antibody in ChipSeq");
    if (isHistoneModAntibody(antibody))
        {
	if (startsWith("H3K36me3", antibody))
	   guess = "exon";
	else if (startsWith("H3K79me2", antibody))
	   guess = "exon";
	}
    }
return guess;
}

char *guessFormatFromFileName(char *fileName)
/* Figure out validateFile format from fileName */
{
char *name = cloneString(fileName);  /* Our own copy to parse. */
if (endsWith(name, ".gz"))
    chopSuffix(name);
char *suffix = strrchr(name, '.');
char *result = "unknown";
if (suffix != NULL)
    {
    suffix += 1;    // Skip over .
    if (sameString(suffix, "bam"))
	result = "bam";
    else if (sameString(suffix, "bed"))
	result = "bed";
    else if (sameString(suffix, "bed9"))
	result = "bed9";
    else if (sameString(suffix, "bigBed"))
	result = "bigBed";
    else if (endsWith(name, "bigWig"))
	result = "bigWig";
    else if (sameString(suffix, "broadPeak"))
	result = "broadPeak";
    else if (sameString(suffix, "fastq"))
        result = "fastq";
    else if (sameString(suffix, "gtf"))
        result = "gtf";
    else if (sameString(suffix, "narrowPeak"))
        result = "narrowPeak";
    }
freeMem(name);
return result;
}

void addGenomeToManifest(char *genome, struct rbTree *expsByIx, FILE *f)
/* Get metaDb table for genome and write out relevant bits of it to f */
{
struct mdbObj *mdbList = getMdbList(genome);
verbose(1, "processing %s\n", genome);
struct mdbObj *mdb;
for (mdb = mdbList; mdb != NULL; mdb = mdb->next)
    {
    /* Grab the fields we want out of the variable list. */
    char *fileName = NULL;
    char *dccAccession = NULL;
    char *replicate = NULL;
    char *composite = NULL;
    char *dataType = NULL;
    char *objType = NULL;
    char *antibody = NULL;
    char *md5sum = NULL;
    struct mdbVar *v;
    for (v = mdb->vars; v != NULL; v = v->next)
         {
	 char *var = v->var, *val = v->val;
	 if (sameString("fileName", var))
	     fileName = val;
	 else if (sameString("dccAccession", var))
	     dccAccession = val;
	 else if (sameString("replicate", var))
	     replicate = val;
	 else if (sameString("composite", var))
	     composite = val;
	 else if (sameString("dataType", var))
	     dataType = val;
	 else if (sameString("objType", var))
	     objType = val;
	 else if (sameString("antibody", var))
	     antibody = val;
	 else if (sameString("md5sum", var))
	     md5sum = val;
	 }

    /* If we have the fields we need,  fake the rest if need be and output. */
    if (fileName != NULL && dccAccession != NULL && objType != NULL)
        {
	if (sameString(objType, "file") || sameString(objType, "table"))
	    {
	    /* Just take first fileName in comma separated list. */
	    char *comma = strchr(fileName, ',');
	    if (comma != NULL) 
		*comma = 0;

	    if (composite == NULL)
	        errAbort("No composite for %s %s", dccAccession, fileName);

	    if (md5sum != NULL)
		{
		char *comma = strchr(md5sum, ',');
		if (comma != NULL) 
		    *comma = 0;
		}

	    /* Output each field. */ 
	    fprintf(f, "%s/%s/%s\t", genome, composite, fileName);
	    fprintf(f, "%s\t", guessFormatFromFileName(fileName));
	    fprintf(f, "%s\t", dccAccession);
	    fprintf(f, "%s\t", naForNull(replicate));
	    fprintf(f, "%s\t", guessEnrichedIn(composite, dataType, antibody));
	    fprintf(f, "%s\n", naForNull(md5sum));
	    }
	}
    }
}

void encode2Manifest(char *outFile)
/* encode2Manifest - Create a encode3 manifest file for encode2 files. */
{
struct rbTree *expsByIx = makeExpContainer();
verbose(1, "%d experiments\n", expsByIx->n);
FILE *f = mustOpen(outFile, "w");
int i;
fputs("#file_name\tformat\texperiment\treplicate\tenriched_in\tmd5sum\n", f);
for (i=0; i<ArraySize(metaDbs); ++i)
   {
   addGenomeToManifest(metaDbs[i], expsByIx, f);
   }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
encode2Manifest(argv[1]);
return 0;
}
