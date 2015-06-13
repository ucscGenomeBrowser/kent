/* liftList - lift a list of files using the liftOver program. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftList - lift a list of files using the liftOver program.\n"
  "    The files should be listed in a .ra file, for each stanza\n"
  "    a file will be lifted.  The file name defaults to the table\n"
  "    name, if no table name is provided the track name is used."    
  "usage:\n"
  "    liftList inputFile.ra\n"
  "options:\n"
  "    -destPath The destination folder, the lifted files will be\n"
  "		dumped here. For lifting files from hg19 to hg38 the\n"
  "		default was /hive/data/genomes/hg38/hg19MassiveLift/wgEncodeReg/\n"
  "    -startPath The folder that holds the files listed in the input\n"
  "		.ra file. For lifting files from hg19 to hg38 the \n"
  "             default was /gbdb/hg19/bbi/ \n"
  "    -chainFile The chain.gz file used with liftOver. For lifting \n"
  "		files from hg19 to hg38 the defauls was  \n"
  "             /hive/data/genomes/hg19/bed/liftOver/hg19ToHg39.over.chain.gz \n"
  "    -chromSizes The chrom.sizes file used with bedGraphToBigWig. For lifting \n"
  "		files from hg19 to hg38 the defauls was  \n"
  "             /hive/data/genomes/hg38/chrom.sizes \n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
   {"destPath", OPTION_STRING},
   {"startPath", OPTION_STRING},
   {"chainFile", OPTION_STRING},
   {"chromSizes", OPTION_STRING},
};

char *cStartPath = "/gbdb/hg19/bbi/";
char *cDestPath = "/hive/data/genomes/hg38/bed/hg19MassiveLift/wgEncodeReg/";
char *cChainFile = "/hive/data/genomes/hg19/bed/liftOver/hg19ToHg38.over.chain.gz";
char *cChromSizes = "/hive/data/genomes/hg38/chrom.sizes";



void liftList(char *input, char *startPath, char *destPath, char *chainFile, char *chromSizes)
/* liftList - lift a list of files using the liftOver program. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
struct slPair *raStanzList = NULL;
for(raStanzList = raNextStanzAsPairs(lf); raStanzList != NULL; raStanzList = raNextStanzAsPairs(lf))
    {
    char *table = "table";
    char *track = "track";
    char *type = "type";
    char *fileName = NULL;
    struct slPair *raStanz = NULL;
    for (raStanz = raStanzList; raStanz != NULL; raStanz = raStanz->next)
        {
	char liftCall[1024];
	if (!strcmp(raStanz->name, track))
	    {
	    fileName = cloneString(raStanz->val);
	    safef(liftCall, sizeof(liftCall), "./liftingScript %s%s.bigWig %s%s.bigWig %s %s",
		 startPath,fileName,destPath,fileName, chainFile, chromSizes);
	    }
	if (!strcmp(raStanz->name,table))
	    {
	    fileName = NULL;
    	    fileName = cloneString(raStanz->val);
	    safef(liftCall, sizeof(liftCall), "./liftingScript %s%s.bigWig %s%s.bigWig %s %s",
		 startPath,fileName,destPath,fileName, chainFile, chromSizes);
	    }
	if (!strcmp(raStanz->name, type))
	    {
	    char *lineScore = NULL;
	    char *query = " 0 ";
	    char *value = cloneString(raStanz->val);
	    lineScore=strtok(value,query);
	    lineScore=strtok(NULL,query);
	    if (strcmp(lineScore,"1")){
	        mustSystem(liftCall);
		//uglyf("The liftCall is %s \n", liftCall);
	        }
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
cDestPath = optionVal("destPath", cDestPath);
cStartPath = optionVal("startPath", cStartPath);
cChromSizes = optionVal("chromSizes", cChromSizes);
cChainFile = optionVal("chainFile", cChainFile);
if (argc != 2)
    usage();
liftList(argv[1], cStartPath, cDestPath, cChainFile, cChromSizes);
return 0;
}
