#include "common.h"
#include <sys/stat.h>
#include <sys/types.h>
#include "synMap.h"
#include "jksql.h"
#include "hdb.h"
#include "dystring.h"

int maxFaSize = 125000;
boolean dontCheckSize = FALSE;
void usage()
{
errAbort("createSlamParaSpec - constructs a spec file for use on parasol\n"
	 "UCSC's kilo-cluster with slam and runSlam.\n"
	 "usage:\n\t"
	 "createSlamParaSpec <resultsDir> <refNibDir> <alignNibDir> <synMapFile> <specOutFile> <db> <makeSubDirs{true|false}> <refPrefix> <alignPrefix> <binPrefix> <skipSizeChecks>\n");
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
     if((synSize >= (int)minSize && synSize <= maxFaSize) || dontCheckSize ) 
 	{ 
 	slAddHead(&tmpList, sm); 
 	} 
    else
	{
	if(synSize < (int)minSize)
	    warn("Size too small for: %s:%d-%d", sm->chromA, sm->chromStartA, sm->chromEndA);
	else if(synSize > maxFaSize)
	    warn("Size too large for: %s:%d-%d", sm->chromA, sm->chromStartA, sm->chromEndA);
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
     dyStringClear(command);
     out1 = stitchName(sm->chromA, sm->chromStartA, sm->chromEndA);
     out2 = stitchName(sm->chromB, sm->chromStartB, sm->chromEndB);
/*  runSlam <chromNibDir> <bitsNibDir> <resultsDir> <chrN:1-10000.gff> <chrN:1-10000.gff> <chromPiece [chrN:1-10000]> <otherBits chrN[1-1000]....> */
     dyStringPrintf(command, "%s/runSlam %s %s %s/%s %s %s {check out exists %s/%s/%s.%s.gff} {check out exists %s/%s/%s.%s.merged.gff} %s %s",
		    binPrefix,
		    refNibDir, alignNibDir, resultsDir, sm->chromA, 
		    refPrefix, alignPrefix,
		    resultsDir, sm->chromA, refPrefix, out1, 
		    resultsDir, sm->chromA, alignPrefix, out2, 
		    out1, 
		    out2);
     fprintf(jobList, "%s/runSlam %s %s %s/%s %s %s  %s/%s/%s.%s.gff %s/%s/%s.%s.merged.gff %s %s",
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
	 alignBit = stitchName(sm->chromB, sm->chromStartB, sm->chromEndB);
	 dyStringPrintf(command, " %s", alignBit);
	 fprintf(jobList, " %s", alignBit);
	 freez(&alignBit);
	 }
     if(command->stringSize < (3*1024))
	 {
	 fprintf(out, "%s\n", command->string);
	 fprintf(jobList, "\n");
	 }
     else
	 {
	 warn("Command %s is too long %d characters, skipping\n",command->string, command->stringSize);
	 }
     freez(&out1);
     freez(&out2);
     }
dyStringFree(&command);
carefulClose(&out);
synMapFreeList(&smList);
}

int main(int argc, char *argv[])
{
if(argc != 12)
    {
    warn("Only %d arguments, expecting 12", argc);
    usage();
    }
jobList = mustOpen("jobList.txt", "w");
if(sameString(argv[11], "true"))
    dontCheckSize = TRUE;
createSlamParaSpec(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9], argv[10]);
carefulClose(&jobList);
return 0;
}
