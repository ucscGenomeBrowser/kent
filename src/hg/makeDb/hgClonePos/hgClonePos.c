/* hgClonePos - create clonePos table in browser database. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "hCommon.h"
#include "jksql.h"
#include "glDbRep.h"
#include "clonePos.h"
#include "gsSeqInfo.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgClonePos - create clonePos table in browser database\n"
  "usage:\n"
  "   hgClonePos [options] database ooDir ffa/sequence.inf gsDir\n"
  "options:\n"
  "   -chromLst=chrom.lst - chromosomes subdirs are named in chrom.lst (1, 2, ...)\n"
  "   -maxErr=N  set maximum allowed errors before aborting (default 0)\n"
  "   -maxWarn=N  set maximum number of warnings to print (default 10)\n"
  "   -verbose=N  set verbose level to N\n"
  "example:\n"
  "   cd /cluster/store5/gs.18/build35\n"
  "   hgClonePos -chromLst=chrom.lst hg17 build35 ./sequence.inf \\\n"
  "       /cluster/store5/gs.18 -maxErr=3 -maxWarn=2000");
}

int maxErr = 0;
int maxWarn = 10;

char *createClonePos = 
NOSQLINJ "CREATE TABLE clonePos (\n"
"   name varchar(255) not null,	# Name of clone including version\n"
"   seqSize int unsigned not null,	# base count not including gaps\n"
"   phase tinyint unsigned not null,	# htg phase\n"
"   chrom varchar(255) not null,	# Chromosome name\n"
"   chromStart int unsigned not null,	# Start in chromosome\n"
"   chromEnd int unsigned not null,	# End in chromosome\n"
"   stage char(1) not null,	# F/D/P for finished/draft/predraft\n"
"   faFile varchar(255) not null,	# File with sequence.\n"
"             #Indices\n"
"   INDEX(name(12)),\n"
"   INDEX(chrom(12),chromStart)\n"
")\n";

void addCloneInfo(char *glFileName, struct hash *cloneHash, struct clonePos **pCloneList)
/* Add in clone info from one .gl file. */
{
char dir[256], chrom[128], ext[64];
struct gl gl;
struct lineFile *lf = lineFileOpen(glFileName, TRUE);
struct clonePos *clone;
char *line, *words[8];
int lineSize, wordCount;
char cloneVerName[128];
char cloneName[128];

printf("Processing %s\n", glFileName);
splitPath(glFileName, dir, chrom, ext);
while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (wordCount != 4)
        errAbort("Expecting %d words line %d of %s", 4, lf->lineIx, lf->fileName);
    glStaticLoad(words, &gl);
    fragToCloneVerName(gl.frag, cloneVerName);
    fragToCloneName(gl.frag, cloneName);
    if ((clone = hashFindVal(cloneHash, cloneName)) == NULL)
        {
	AllocVar(clone);
	clone->name = cloneString(cloneVerName);
	clone->chrom = cloneString(chrom);
	clone->chromStart = gl.start;
	clone->chromEnd = gl.end;
	clone->faFile = cloneString("");
	slAddHead(pCloneList, clone);
	hashAdd(cloneHash, cloneName, clone);
	}
    else
        {
	if (!sameString(clone->chrom, chrom))
	    {
	    warn("Clone %s is on chromosomes %s and %s.  Ignoring %s",
	        cloneName, clone->chrom, chrom, chrom);
	    continue;
	    }
	if (clone->chromStart > gl.start)
	    clone->chromStart = gl.start;
	if (clone->chromEnd < gl.end)
	    clone->chromEnd = gl.end;
	}
    }
lineFileClose(&lf);
}

int cmpClonePos(const void *va, const void *vb)
/* Compare to sort based on chromosome and chromStart. */
{
const struct clonePos *a = *((struct clonePos **)va);
const struct clonePos *b = *((struct clonePos **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}

struct clonePos *readClonesFromOoDir(char *ooDir, struct hash *cloneHash)
/* Read in clones from ooDir. */
{
struct clonePos *cloneList = NULL;
struct fileInfo *chrFiList = NULL, *chrFi; 
struct fileInfo *glFiList = NULL, *glFi;
char pathName[512];
struct hash *chromDirHash = newHash(4);
char *chromLst = optionVal("chromLst", NULL);

if (chromLst != NULL)
    {
    struct lineFile *clf = lineFileOpen(chromLst, TRUE);
    char *row[1];
    while (lineFileRow(clf, row))
        {
        hashAdd(chromDirHash, row[0], NULL);
	verbose(3,"%s\n",row[0]);
        }
    lineFileClose(&clf);
    }

verbose(2,"ooDir: %s\n",ooDir);
chrFiList = listDirX(ooDir, "*", FALSE);
for (chrFi = chrFiList; chrFi != NULL; chrFi = chrFi->next)
    {
verbose(2,"%s\n",chrFi->name);
    if ( ((chrFi->isDir && strlen(chrFi->name) <= 2))
	|| hashLookup(chromDirHash, chrFi->name) )
        {
	sprintf(pathName, "%s/%s", ooDir, chrFi->name);
	verbose(2,"%s\n",pathName);
	glFiList = listDirX(pathName, "*.gl", TRUE);
	for (glFi = glFiList; glFi != NULL; glFi = glFi->next)
	    addCloneInfo(glFi->name, cloneHash, &cloneList);
	slFreeList(&glFiList);
        }
    }
slFreeList(&chrFiList);
slReverse(&cloneList);
slSort(&cloneList, cmpClonePos);
if (slCount(cloneList) < 0)
   errAbort("No .gl files in %s\n", ooDir);
printf("Got %d clones\n", slCount(cloneList));
hashFree(&chromDirHash);
return cloneList;
}

void addStageInfo(char *gsDir, struct hash *cloneHash)
/* Add info about which file and what stage clone is in. */
/* TSF - This is no longer used due to unavailability of *.finf files - 4/7/2003 */
{
static char *finfFiles[] = {"ffa/finished.finf", "ffa/draft.finf",
			    "ffa/predraft.finf", "ffa/extras.finf"};
static char stages[] = "FDPD";
struct lineFile *lf;
char *line;
char *words[7];
int numStages = strlen(stages);
int i;
char pathName[512];
char *finfFile, stage;
int warnsLeft = maxWarn; /* Only show first maxWarn warnings about missing clones. */
char cloneName[256];
struct clonePos *clone;
int wordCount, cloneCount;

for (i=0; i<numStages; ++i)
   {
   finfFile = finfFiles[i];
   stage = stages[i];
   sprintf(pathName, "%s/%s", gsDir, finfFile);
   printf("Processing %s\n", pathName);
   lf = lineFileOpen(pathName, TRUE);
   cloneCount = 0;
   while (lineFileNext(lf, &line, NULL))
       {
       wordCount = chopLine(line, words);
       assert(wordCount == 7);
       strncpy(cloneName, words[1], sizeof(cloneName));
       chopSuffix(cloneName);
       if ((clone = hashFindVal(cloneHash, cloneName)) == NULL)
            {
	    if (warnsLeft > 0)
	       {
	       --warnsLeft;
	       warn("%s is in %s but not in ooDir/*/*.gl", cloneName, pathName);
	       }
	    else if (warnsLeft == 0)
		{
		--warnsLeft;
		warn("(Truncating additional warnings)");
		}
	    continue;
	    }
       clone->stage[0] = stage;
       cloneCount++;
       }
   lineFileClose(&lf);
   printf("Got %d clones in %s\n", cloneCount, pathName);
   }
}

void addSeqInfo(char *seqInfoName, struct hash *cloneHash)
/* Add in information from sequence.info file. */
/* TSF - stage info now being derived from here - 4/7/2003 */
{
struct lineFile *lf = lineFileOpen(seqInfoName, TRUE);
char *line, *words[16];
int lineSize, wordCount;
struct clonePos *clone;
struct gsSeqInfo gs;
int warnsLeft = maxWarn;  /* Only show first maxWarn warnings about missing clones. */
static char stages[] = "PPDF";

printf("Processing %s\n", seqInfoName);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (wordCount < 8)
        errAbort("Expecting 8 words line %d of %s", lf->lineIx, lf->fileName);
    gsSeqInfoStaticLoad(words, &gs);
    /* TSF - Phase 0 clones now included in contig_overlaps.agp 4/23/2003 */
    /*if (gs.phase != 0)
      {*/
    chopSuffix(gs.acc);
    if ((clone = hashFindVal(cloneHash, gs.acc)) == NULL)
	{
	if (warnsLeft > 0)
	    {
	    --warnsLeft;
	    warn("%s is in %s but not in ooDir/*/*.gl", gs.acc, seqInfoName);
	    }
	else if (warnsLeft == 0)
	    {
	    --warnsLeft;
	    warn("(Truncating additional warnings)");
	    }
	continue;
	}
    clone->seqSize = gs.size;
    clone->phase = gs.phase;
    clone->stage[0] = stages[atoi(words[3])];
    /* } */
    }
lineFileClose(&lf);
}

void checkClonePos(struct clonePos *cloneList)
/* Make sure that all necessary bits of cloneList are filled in. */
{
struct clonePos *clone;
int errCount = 0;

for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    if (clone->seqSize == 0)
        {
	if (errCount < 20)
	    warn("Missing size info on %s (not in sequence.inf)", clone->name);
	++errCount;
	}
    if (clone->stage[0] == 0)
        {
	if (errCount < 20)
	    warn("Missing stage info on %s (not in *.finf)", clone->name);
        clone->stage[0] = 'D';
	++errCount;
	}
    }
if (errCount > maxErr)
    {
    errAbort("Aborting with %d errors\n", errCount);
    }
}

void saveClonePos(struct clonePos *cloneList, char *database)
/* Save sorted clone position list to database. */
{
struct sqlConnection *conn = sqlConnect(database);
struct clonePos *clone;
struct tempName tn;
FILE *f;
struct dyString *ds = newDyString(2048);

/* Create tab file from clone list. */
printf("Creating tab file\n");
makeTempName(&tn, "hgCP", ".tab");
f = mustOpen(tn.forCgi, "w");
for (clone = cloneList; clone != NULL; clone = clone->next)
    clonePosTabOut(clone, f);
fclose(f);

/* Create table if it doesn't exist, delete whatever is
 * already in it, and fill it up from tab file. */
printf("Loading clonePos table\n");
sqlMaybeMakeTable(conn, "clonePos", createClonePos);
sqlUpdate(conn, NOSQLINJ "DELETE from clonePos");
sqlDyStringPrintf(ds, "LOAD data local infile '%s' into table clonePos", 
    tn.forCgi);
sqlUpdate(conn, ds->string);

/* Clean up. */
remove(tn.forCgi);
sqlDisconnect(&conn);
}

void hgClonePos(char *database, char *ooDir, char *seqInfoName, char *gsDir)
/* hgClonePos - create clonePos table in browser database. */
{
struct hash *cloneHash = newHash(16);
struct clonePos *cloneList = NULL;

cloneList = readClonesFromOoDir(ooDir, cloneHash);
/* addStageInfo(gsDir, cloneHash); */
addSeqInfo(seqInfoName, cloneHash);
checkClonePos(cloneList);
saveClonePos(cloneList, database);
freeHash(&cloneHash);
clonePosFreeList(&cloneList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
maxErr = optionInt("maxErr", maxErr);
maxWarn = optionInt("maxWarn", maxWarn);
hgClonePos(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
