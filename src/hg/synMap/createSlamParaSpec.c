#include "common.h"
#include <sys/stat.h>
#include <sys/types.h>
#include "synMap.h"
#include "jksql.h"
#include "hdb.h"
#include "dystring.h"

void usage()
{
errAbort("createSlamParaSpec - constructs a spec file for use on parasol\n"
	 "UCSC's kilo-cluster with slam and runSlam.\n"
	 "usage:\n\t"
	 "createSlamParaSpec <resultsDir> <refNibDir> <alignNibDir> <synMapFile> <specOutFile> <db> <makeSubDirs{true|false}> <refPrefix> <alignPrefix> <binPrefix\n");
}

char *stitchName(char *chrom, unsigned int start, unsigned int end)
/* stitch chrom, start, end into chrom:start-end */
{
char buff[512];
if(strstr(chrom, "chr") != NULL)
    snprintf(buff, sizeof(buff), "%s:%u-%u", chrom, start, end);
else
    snprintf(buff, sizeof(buff), "chr%s:%u-%u", chrom, start, end);
return cloneString(buff);
}

void makeSubDirs(char *db)
/* create a subdir for each chrom in database */
{
struct sqlConnection *conn = NULL;
struct sqlResult *sr = NULL;
char **row;
char *query = "select chrom from chromInfo";
hSetDb(db);
conn = hAllocConn();
sr = sqlGetResult(conn, query);
while((row = sqlNextRow(sr)) != NULL)
    {
    mkdir(row[0], 1);
    chmod(row[0],0777);
    }
mkdir("log", 1);
chmod("log",0777);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void outputRequieredFiles(struct synMap *smList, char *refPrefix, char *alignPrefix, char *resultsDir)
{
FILE *out = mustOpen("requiredFiles.txt","w");
struct synMap *sm = NULL;
char *out1 = NULL, *out2 = NULL;
for(sm = smList; sm != NULL; sm = sm->next)
    {
    out1 = stitchName(sm->chromA, sm->chromStartA, sm->chromEndA);
    fprintf(out, "%s/%s/%s.%s.gff\n", 	    resultsDir, sm->chromA, refPrefix, out1);
    freez(&out1);
    }
carefulClose(&out);
}

void createSlamParaSpec(char *resultsDir, char *refNibDir, char *alignNibDir, char *synMapFile, char *specOut, char *db, 
			char *subDirs, char *refPrefix, char *alignPrefix, char *binPrefix)
/* top level function to write everything out */
{
struct synMap *smList = NULL, *sm = NULL;
struct synMap *tmpList = NULL, *smNext = NULL;

char *out1 = NULL, *out2 = NULL;
FILE *out = NULL;
int synSize = 0;
float minSize = 0;
smList = synMapLoadAll(synMapFile);
out = mustOpen(specOut, "w");
if(sameString(subDirs,"true") || sameString(subDirs,"TRUE"))
    makeSubDirs(db);
for(sm = smList; sm != NULL; sm = smNext)
    {
    minSize = .05 * (sm->chromEndA - sm->chromStartA);
    synSize = sm->chromEndB - sm->chromStartB;
    smNext = sm->next;
    if(synSize > (int)minSize)
	{
	slAddHead(&tmpList, sm);
	}
    else
	{
	synMapFree(&sm);
	}
    }
slReverse(&tmpList);
smList = tmpList;
for(sm = smList; sm != NULL; sm = sm->next)
     {
     
     out1 = stitchName(sm->chromA, sm->chromStartA, sm->chromEndA);
     out2 = stitchName(sm->chromB, sm->chromStartB, sm->chromEndB);
/*  runSlam <chromNibDir> <bitsNibDir> <resultsDir> <chrN:1-10000.gff> <chrN:1-10000.gff> <chromPiece [chrN:1-10000]> <otherBits chrN[1-1000]....> */
    fprintf(out, "%s/runSlam %s %s %s/%s %s %s {check out exists %s/%s/%s.%s.gff} {check out exists %s/%s/%s.%s.gff} %s %s\n",
	    binPrefix,
	    refNibDir, alignNibDir, resultsDir, sm->chromA, 
	    refPrefix, alignPrefix,
	    resultsDir, sm->chromA, refPrefix, out1, 
	    resultsDir, sm->chromA, alignPrefix, out2, 
	    out1, 
	    out2);
    freez(&out1);
    freez(&out2);
    }
carefulClose(&out);
synMapFreeList(&smList);
}

int main(int argc, char *argv[])
{
if(argc < 10)
    usage();
createSlamParaSpec(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9], argv[10]);
return 0;
}
