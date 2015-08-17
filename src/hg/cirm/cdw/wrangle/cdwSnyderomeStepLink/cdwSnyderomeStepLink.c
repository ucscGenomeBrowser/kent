/* cdwSnyderomeStepLink - Help fill out cdwStep tables for snyderome test set. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "cdwStep.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwSnyderomeStepLink - Help fill out cdwStep tables for snyderome test set\n"
  "usage:\n"
  "   cdwSnyderomeStepLink output.sql\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct slInt *getWgsData(struct sqlConnection *conn, char *format)
/* Get list of file IDs for fastq files from snyderome */
{
char query[1024];
sqlSafef(query, sizeof(query),
    "select file_id from cdwFileTags where format='%s' and data_set_id='Snyderome' "
    " and assay='WGS'", format);
return sqlQuickNumList(conn, query);
}

struct slName *getChromList(struct sqlConnection *conn)
/* Get all chroms used by snyderome */
{
char query[1024];
sqlSafef(query, sizeof(query), 
   "select distinct(chrom) from cdwFileTags where chrom like 'chr%%' and data_set_id='Snyderome'");
return sqlQuickList(conn, query);
}

void cdwSnyderomeStepLink(char *sqlOutput)
/* cdwSnyderomeStepLink - Help fill out cdwStep tables for snyderome test set. */
{
struct sqlConnection *conn = cdwConnect();
struct slInt *fastqList = getWgsData(conn, "fastq");
struct slInt *bamList = getWgsData(conn, "bam");

FILE *f = mustOpen(sqlOutput, "w");

/* Loop through and create input records */
int stepRunId = 2;   // Just entered this manually
struct slInt *fastq;
int ix = 0;
for (fastq = fastqList; fastq != NULL; fastq = fastq->next)
    {
    fprintf(f, "insert into cdwStepIn (stepRunId,name,ix,fileId) values "
	" (%d,'reads',%d, %d);\n"
	, stepRunId, ix, fastq->val);
    ++ix;
    }

/* Create output records */
struct slInt *bam;
ix = 0;
for (bam = bamList; bam != NULL; bam = bam->next)
    {
    fprintf(f, "insert into cdwStepOut (stepRunId,name,ix,fileId) values "
	" (%d,'alignments',%d, %d);\n"
	, stepRunId, ix, bam->val);
    ++ix;
    }

struct slName *chrom, *chromList = getChromList(conn);
int vcfStepDef = 4;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
     {
     ++stepRunId;   // Just entered this manually
     fprintf(f, "insert into cdwStepRun (stepDef,stepVersion) values (%d,'unknown');\n",
	vcfStepDef);


     /* Figure out the bam */
     char query[1024];
     sqlSafef(query, sizeof(query),
	"select file_id from cdwFileTags where chrom='%s' "
	"  and format='BAM' and data_set_id='Snyderome' and assay='WGS'"
	, chrom->name);
     int bamId = sqlQuickNum(conn, query);
     if (bamId == 0)
         errAbort("Can't find bam for %s", chrom->name);

     /* Get the VCFs, there should be three */
     sqlSafef(query, sizeof(query),
        "select file_id,submit_file_name from cdwFileTags where chrom='%s' "
	"  and format='VCF' and data_set_id='Snyderome' and assay='WGS'"
	, chrom->name);
     struct sqlResult *sr = sqlGetResult(conn, query);
     char **row;
     fprintf(f, "insert into cdwStepIn (stepRunId,name,ix,fileId) values (%d,'%s', 0, %d);\n"
           , stepRunId, "alignments", bamId);
     while ((row = sqlNextRow(sr)) != NULL)
         {
	 int vcfId = sqlUnsigned(row[0]);
	 char *fileName = row[1];
	 char *outName = NULL;
	 if (endsWith(fileName, ".snp.raw.vcf.gz"))
	     outName = "SNP";
	 else if (endsWith(fileName, ".indel.raw.vcf.gz"))
	     outName = "indel";
	 else if (endsWith(fileName, ".dindel.indel.vcf.gz"))
	     outName = "dindel";
	 else
	     errAbort("Unrecognized file ending in %s", fileName);
	fprintf(f, "insert into cdwStepOut (stepRunId,name,ix,fileId) values "
	    " (%d, '%s', 0, %d);\n"
	    , stepRunId, outName, vcfId);
	 }
     sqlFreeResult(&sr);
     }

carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwSnyderomeStepLink(argv[1]);
return 0;
}
