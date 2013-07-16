#include "common.h"
#include <sys/stat.h>
#include <sys/types.h>
#include "synMap.h"
#include "jksql.h"
#include "hdb.h"
#include "dystring.h"


int maxFaSize = BIGNUM;
boolean dontCheckLargeSize = FALSE;
boolean dontCheckSmallSize = FALSE;
void usage()
{
errAbort("createSlamParaSpec - constructs a spec file for use on parasol\n"
	 "UCSC's kilo-cluster with slam and runSlam.\n"
	 "usage:\n\t"
	 "createSlamParaSpec <maxFaSize> <resultsDir> <refNibDir> <alignNibDir> <synMapFile> <specOutFile> <db> <makeSubDirs{true|false}> <refPrefix> <alignPrefix> <binPrefix> <skipLargeSizeChecks> <skipSmallSizeChecks>\n");
}

FILE *jobList = NULL;
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
char *query = "NOSQLINJ select chrom from chromInfo";
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

void createSlamParaSpec(int maxFaSize, char *resultsDir, char *refNibDir, char *alignNibDir, char *synMapFile, char *specOut, char *db, 
			char *subDirs, char *refPrefix, char *alignPrefix, char *binPrefix)
/* top level function to write everything out */
{
struct synMap *smList = NULL, *sm = NULL;
struct synMap *tmpList = NULL, *smNext = NULL;
struct dyString *command = newDyString(4096);
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
     if((synSize >= (int)minSize || dontCheckSmallSize))
 	{ 
 	slAddHead(&tmpList, sm); 
 	} 
    else
	{
	if(synSize < (int)minSize && !dontCheckSmallSize)
	    warn("Size too small for: %s:%d-%d", sm->chromA, sm->chromStartA, sm->chromEndA);
	else
	    errAbort("createSlamParaSpec::createSlamParaSpec() - Confused, shouldn't be aborting for : %s:%d-%d but I am...", 
		     sm->chromA, sm->chromStartA, sm->chromEndA);
	synMapFree(&sm);
	}
    }
slReverse(&tmpList);
smList = tmpList;
for(sm = smList; sm != NULL; sm = sm->next)
    {
    int faTotal = 0;
    faTotal += sm->chromEndB - sm->chromStartB;
    dyStringClear(command);
    out1 = stitchName(sm->chromA, sm->chromStartA, sm->chromEndA);
    out2 = stitchName(sm->chromB, sm->chromStartB, sm->chromEndB);
/* switch back to this command when we are sure that .gff file will be produced every time. */
/*     dyStringPrintf(command, "%s/runSlam %s %s %s/%s %s %s {check out exists %s/%s/%s.%s.gff} {check out exists %s/%s/%s.%s.merged.gff} %s %s", */
/* 		   binPrefix, */
/* 		   refNibDir, alignNibDir, resultsDir, sm->chromA,  */
/* 		   refPrefix, alignPrefix, */
/* 		   resultsDir, sm->chromA, refPrefix, out1,  */
/* 		   resultsDir, sm->chromA, alignPrefix, out2,  */
/* 		   out1,  */
/* 		   out2); */
    dyStringPrintf(command, "%s/runSlam %d %s %s %s/%s %s %s {check out exists %s/%s/%s.%s_%s.%s.log} dummy.txt %s %s",
		   maxFaSize,
		   binPrefix,
		   refNibDir, alignNibDir, resultsDir, sm->chromA, 
		   refPrefix, alignPrefix,
		   resultsDir, sm->chromA, refPrefix, out1, 
		   alignPrefix, out2, 
		   out1, 
		   out2);
    fprintf(jobList, "%s/runSlam %d %s %s %s/%s %s %s  %s/%s/%s.%s.gff %s/%s/%s.%s.merged.gff %s %s",
	    maxFaSize,
	    binPrefix,
	    refNibDir, alignNibDir, resultsDir, sm->chromA, 
	    refPrefix, alignPrefix,
	    resultsDir, sm->chromA, refPrefix, out1, 
	    resultsDir, sm->chromA, alignPrefix, out2, 
	    out1, 
	    out2);
    while(sm->next && sameString(sm->next->chromA, sm->chromA) 
	  && sm->next->chromStartA == sm->chromStartA && sm->next->chromEndA == sm->chromEndA)
	{
	char *alignBit = NULL;
	sm = sm->next;
	faTotal += sm->chromEndB - sm->chromStartB;
	alignBit = stitchName(sm->chromB, sm->chromStartB, sm->chromEndB);
	dyStringPrintf(command, " %s", alignBit);
	fprintf(jobList, " %s", alignBit);
	freez(&alignBit);
	}
    if(command->stringSize < (3.5*1024) && 
       (faTotal <= maxFaSize || dontCheckLargeSize) )
	{
	fprintf(out, "%s\n", command->string);
	fprintf(jobList, "\n");
	}
    else if(command->stringSize >= (3.5*1024))
	warn("Command %s is too long %d characters, skipping\n",command->string, command->stringSize);
    else
	warn("Size too large for: %s:%d-%d, %d counted, %d allowed", sm->chromA, sm->chromStartA, sm->chromEndA,
	     faTotal, maxFaSize);
    freez(&out1);
    freez(&out2);
    }
dyStringFree(&command);
carefulClose(&out);
synMapFreeList(&smList);
}

int main(int argc, char *argv[])
{
if(argc != 14)
    {
    warn("Only %d arguments, expecting 14", argc);
    usage();
    }
jobList = mustOpen("jobList.txt", "w");
if(sameString(argv[12], "true"))
    dontCheckLargeSize = TRUE;
if(sameString(argv[13], "true"))
    dontCheckSmallSize = TRUE;
createSlamParaSpec(atoi(argv[1]), argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], argv[11]);
carefulClose(&jobList);
return 0;
}
