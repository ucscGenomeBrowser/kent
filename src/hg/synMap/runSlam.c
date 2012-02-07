#include "common.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include "nib.h"
#include "fa.h"
#include "dnaseq.h"
#include "dystring.h"
void usage()
{
errAbort("runSlam - runs the Slam program program given a reference chromosome\n"
	 "and some aligned bits. Now does chdir().\n"
	 "usage:\n\t"
	 "runSlam <maxFaSize> <chromNibDir> <bitsNibDir> <resultsDir> <refPrefix> <alignPrefix> <chrN:1-10000.gff> <chrN:1-10000.gff> <chromPiece [chrN:1-10000]> <otherBits chrN[1-1000]....>\n");
}
char *slamBin = "/cluster/home/sugnet/slam/";
char *repeatMaskBin = "/scratch/hg/RepeatMasker/";
char *outputRoot = "/tmp/slam/";
boolean runAvidFirst = TRUE; /* if true run avid first to order and orient fasta files. */
char *slamOpts = NULL; /* Options to pass to slam.pl */
int bpLimit = 400000; /* maximum nubmer of base pairs to allow in a fasta file. */


struct genomeBit
/* Piece of the genome */
{
    struct genomeBit *next;
    char *fileName;
    char *chrom;
    int chromStart;
    int chromEnd;
};

char *fileSuffixes[] = { ".fa", ".fa.cat", ".fa.masked", ".fa.masked.log", ".fa.out", ".fa.stderr", ".fa.tbl", ".fa.mout", ".fa.minfo"};

char *slamGetHost()
/* Return host name. */
{
static char *hostName = NULL;
static char buf[128];
if (hostName == NULL)
    {
    hostName = getenv("HTTP_HOST");
    if (hostName == NULL)
        {
	hostName = getenv("HOST");
	if (hostName == NULL)
	    {
	    if (hostName == NULL)
		{
		static struct utsname unamebuf;
		if (uname(&unamebuf) >= 0)
		    hostName = unamebuf.nodename;
		else
		    hostName = "unknown";
		}
	    }
        }
    strncpy(buf, hostName, sizeof(buf));
    chopSuffix(buf);
    hostName = buf;
    }
return hostName;
}

char *slamTempName(char *dir, char *base, char *suffix)
/* Make a temp name that's almost certainly unique. */
{
char midder[256];
int pid = getpid();
int num = time(NULL);
static char fileName[512];
char host[512];
char *s;

strcpy(host, slamGetHost());
s = strchr(host, '.');
if (s != NULL)
     *s = 0;
for (;;)
   {
   sprintf(fileName, "%s/%s_%s_%d_%d%s", dir, base, host, pid, num, suffix);
   if (!fileExists(fileName))
       break;
   num += 1;
   }
return fileName;
}

void genomeBitFree(struct genomeBit **pGb)
/* free a single genome bit */
{
struct genomeBit *gb = NULL;
if ((gb = *pGb) == NULL) return;
freez(&gb->fileName);
freez(&gb->chrom);
}

void genomeBitFreeList(struct genomeBit **pList)
/* Free a list of dynamically allocated genomeBits */
{
struct genomeBit *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    genomeBitFree(&el);
    }
*pList = NULL;
}

struct genomeBit *parseGenomeBit(char *name)
/* take a name like chrN:1-1000 and parse it into a genome bit */
{
struct genomeBit *gp = NULL;
char *pos1 = NULL, *pos2 = NULL;
AllocVar(gp);
pos1 = strstr(name, ":");
 if(pos1 == NULL)
   errAbort("runSlam::parseGenomeBit() - %s doesn't look like chrN:1-1000", name);
gp->chrom = name;
*pos1 = '\0';
gp->chrom = cloneString(gp->chrom);
pos1++;
pos2 = strstr(pos1, "-");
 if(pos2 == NULL)
   errAbort("runSlam::parseGenomeBit() - %s doesn't look like chrN:1-1000, name");
*pos2 = '\0';
pos2++;
gp->chromStart = atoi(pos1);
gp->chromEnd = atoi(pos2);
return gp;
}

struct genomeBit *parseGenomeBits(char **positions, int positionCount)
/* construct a list of genomeBits from an array of positions */
{
struct genomeBit *gb = NULL, *gbList = NULL;
int i;
for(i=0; i<positionCount; i++)
    {
    gb = parseGenomeBit(positions[i]);
    slAddHead(&gbList, gb);
    }
slReverse(&gb);
return gb;
}

void makeTempDir()
/* create a temporary directory on local disk for doing i/o work */
{
mkdir(outputRoot, 1);
chmod(outputRoot, 0777);
}

char *fileNameFromGenomeBit(char *root, char *suffix, struct genomeBit *gb)
/* return something like outputRoot/chrN:1-1000.fa given "fa" as the suffix, free with freez() */
{
char fileName[1024];
assert(suffix);
snprintf(fileName, sizeof(fileName), "%s%s:%u-%u%s", root, gb->chrom, gb->chromStart, gb->chromEnd, suffix);
return cloneString(fileName);
}

void removeTempDir(struct genomeBit *target, struct genomeBit *align)
/* remove the files in the temp dir and then the temp dir itself */
{
struct genomeBit *gb = NULL;
char *file = NULL;
char buff[2048];
int retVal = 0;
int i=0;
char *alignSuffixes[] = { ".fa.aat", ".fa.minfo", ".fa.mout" };
for(i=0; i<ArraySize(fileSuffixes); i++)
    {
    file = fileNameFromGenomeBit(outputRoot, fileSuffixes[i], target);
    remove(file);
    freez(&file);
    }
for(i=0; i<ArraySize(fileSuffixes); i++)
    {
    file = fileNameFromGenomeBit(outputRoot, fileSuffixes[i], align);
    remove(file);
    freez(&file);
    }
for(i=0; i<ArraySize(alignSuffixes); i++)
    {
      char *tmpFile = NULL;
    file = fileNameFromGenomeBit(outputRoot, "", target);
    tmpFile = cloneString(file);
    ExpandArray(file, strlen(file), (2*strlen(file)+100));
    snprintf(file, (2*strlen(file)+100), "%s_%s:%u-%u%s", tmpFile, align->chrom, align->chromStart, align->chromEnd,alignSuffixes[i]);
    remove(file);
    freez(&file);
    freez(&file);
    }
snprintf(buff, sizeof(buff),"%score", outputRoot);
remove(buff);
snprintf(buff, sizeof(buff),"%s*", outputRoot);
remove(buff);
rmdir(outputRoot);
snprintf(buff, sizeof(buff), "rm -rf %s", outputRoot);
warn("Executing '%s' just to be sure.", buff);
retVal = system(buff);
warn( "%s exited with value %d", buff, retVal);
}

char *nibFileFromChrom(char *root, char *chromName)
/* return the nib file given the rootDir and chrom, free with freez() */
{
char fileName[1024];
assert(root && chromName);
snprintf(fileName, sizeof(fileName), "%s/%s.nib", root, chromName);
return cloneString(fileName);
}

void repeatMaskFile(char *outDir, struct genomeBit *gb)
/* use this on files that are repeat masked via case, creates
   dummy.masked and dummy.masked out files for slam. */
{
char *fileName = fileNameFromGenomeBit("",".fa", gb);
char buff[2048];
int retVal = 0;
snprintf(buff, sizeof(buff), "%sRepeatMasak %s ",  repeatMaskBin, fileName);
warn("running %s", buff);
retVal = system(buff);
warn("%s exited with value %d", buff, retVal);
freez(&fileName);
}

void fakeRepeatMaskFile(char *outDir, struct genomeBit *gb)
/* use this on files that are repeat masked via case, creates
   dummy.masked and dummy.masked out files for slam. */
{
char *fileName = fileNameFromGenomeBit("",".fa", gb);
char buff[2048];
int retVal = 0;
snprintf(buff, sizeof(buff), "%sprep_lc_rm_for_slam %s ", slamBin, fileName);
warn("running %s", buff);
retVal = system(buff);
warn("%s exited with value %d", buff, retVal);
freez(&fileName);
}

void createFastaFilesForBits(char *root, struct genomeBit *gbList, boolean addDummy)
/* load all of the fasta records for the bits in the genome list into one
   fasta file. Uses .nib files as they are much more compact and allow random access. */
{
struct dnaSeq *seq = NULL;
struct genomeBit *gb = NULL;
FILE *faOut = NULL;
char *faFile = NULL;
char *nibFile = NULL;
int totalBp = 0;
assert(gbList);
faFile = fileNameFromGenomeBit(outputRoot, ".fa", gbList);
faOut = mustOpen(faFile, "w");
for(gb = gbList; gb != NULL; gb = gb->next)
    {
    char buff[256];
    snprintf(buff, sizeof(buff), "%s:%u-%u", gb->chrom, gb->chromStart, gb->chromEnd);
    nibFile = nibFileFromChrom(root, gb->chrom);
    seq = nibLoadPartMasked(NIB_MASK_MIXED, nibFile, gb->chromStart, gb->chromEnd-gb->chromStart);
    totalBp += strlen(seq->dna);

    faWriteNext(faOut, buff, seq->dna, seq->size);
    dnaSeqFree(&seq);
    freez(&nibFile);
    }
/* Add a dummy fasta record so that avid will order and orient things for us.. */
if(addDummy)
    faWriteNext(faOut, "garbage", "nnnnnnnnnn", 10);
carefulClose(&faOut);
/** This bit is commented out as we are now using nnnn's as repeat masking */
/*  if(slCount(gbList) > 1) */
/*      repeatMaskFile(outputRoot, gbList); */
/*  else */
/*    fakeRepeatMaskFile(outputRoot, gbList); */
freez(&faFile);
}

void touchFile(char *root, char *suffix)
/* same functionality as the "touch" unix command. */
{
struct dyString *file = newDyString(2048);
FILE *touch = NULL;
dyStringPrintf(file, "%s%s", root, suffix);
touch = mustOpen(file->string, "w");
carefulClose(&touch);
dyStringFree(&file);
}

void runSlamExe(char *outputDir, struct genomeBit *target, struct genomeBit *aligns, char *refPrefix, char *alignPrefix)
/* run the actual slam program */
{
char *fa1 = NULL;
char *fa2 = NULL;
char *gff1 = NULL;
char *gff2 = NULL;
struct dyString *dy = newDyString(1024);

char command[2048];
int retVal = 0;
/* get our arguments */
fa1 = fileNameFromGenomeBit("", ".fa", target);
fa2 = fileNameFromGenomeBit("", ".fa", aligns);
gff1 = fileNameFromGenomeBit("", "", target);
gff2 = fileNameFromGenomeBit("", "", aligns);

if(runAvidFirst)
    {
    struct dnaSeq *faMerged = NULL, *fa = NULL;
    int bpCount = 0;
    snprintf(command, sizeof(command), "%savid -nm=both  %s %s",
	       slamBin, fa1, fa2); 
    retVal = system(command);

    warn("%s exited with value %d", command, retVal);
    snprintf(command, sizeof(command), "echo \"\" >> %s.merged", fa2);
    system(command);
    dyStringClear(dy);
    dyStringPrintf(dy, "%s.merged", fa2); 

    /* Check to make sure that the merged fa file is smaller than our limit. */
    faMerged = faReadAllDna(dy->string);
    for(fa = faMerged; fa !=NULL; fa = fa->next)
	bpCount += fa->size;
    if( bpCount >= bpLimit)
	{
	warn("runSlam()::runSlam() - trying to write %d to fasta file which is greater than limit of %d for %s\n pair is: %s/%s.%s\t%s/%s.%s",
		 bpCount, bpLimit, dy->string, target->chrom, refPrefix, fa1, target->chrom, alignPrefix, fa2);
	exit(0); // Exit zero to let parasol know we did what we could....
	}
    dyStringClear(dy);
    }
/* File is hardmasked already, create fake repeat-masker files so other
   programs don't squawk. */
touchFile(fa1, ".out");
touchFile(fa1, ".masked");
touchFile(fa2, ".merged.out");
touchFile(fa2, ".merged.masked");

if(runAvidFirst) /* if we ran avid first then we need to use .merged files. */
    {
      snprintf(command, sizeof(command),  
  	     "%sslam.pl %s %s %s.merged -outDir %s -o1 %s.%s -o2 %s.%s.merged",   
  	     slamBin, slamOpts, fa1, fa2, outputDir, refPrefix, gff1, alignPrefix, gff2); 
    }
else 
    {
      snprintf(command, sizeof(command),  
  	     "%sslam.pl %s %s %s -outDir %s -o1 %s.%s -o2 %s.%s",   
  	     slamBin, slamOpts, fa1, fa2, outputDir, refPrefix, gff1, alignPrefix, gff2); 
    }
warn("Running %s", command);
retVal = system(command);
warn("%s exited with value %d", command, retVal);

/* cleanup */
freez(&fa1);
freez(&fa2);
freez(&gff1);
freez(&gff2);
}

void runSlam(char *chrNibDir, char *alignNibDir, char *outputDir, char *refPrefix, char *alignPrefix,  char *pos, char **alignBits, int numAlignBits)
/* Top level function. Pipeline is to cut out genome bits, order and orient using avid, align merged using avid, then run slam. */
{
char *tmpDir = NULL;
struct genomeBit *target = parseGenomeBit(pos);
struct genomeBit *aligns = parseGenomeBits(alignBits, numAlignBits);
char *fa1 = NULL;
char *fa2 = NULL;
char *gff1 = NULL;
char *gff2 = NULL;
char buff[4096];
char cwdBuff[4096];
char *cwd = NULL;
int fileNo = 0, stderrNo = 0, stdoutNo =0;
FILE *logFile = NULL;
char *host = NULL;

/* quick hack to reverse args for slam.pl */
if(sameString(refPrefix, "mm"))
    slamOpts = " --org1 M.musculus --org2 H.sapiens ";
else
    slamOpts = "";

/* create file names we're going to be using */
fa1 = fileNameFromGenomeBit("", ".fa", target);
fa2 = fileNameFromGenomeBit("", ".fa", aligns);
gff1 = fileNameFromGenomeBit("", "", target);
gff2 = fileNameFromGenomeBit("", "", aligns);
runAvidFirst = TRUE;  /* Run avid to order and orient before using to align. */

/* We're going to create a temporary working directory and move the program there. */
tmpDir = slamTempName("/tmp", "slam", "/");
outputRoot = tmpDir;
makeTempDir();
cwd = getcwd(cwdBuff, ArraySize(cwdBuff));
assert(cwd);
chdir(outputRoot);
snprintf(buff, sizeof(buff), "%s/%s.%s_%s.%s.log", outputDir, refPrefix, gff1, alignPrefix, gff2);
logFile = mustOpen(buff, "w");
fileNo = fileno(logFile);
stderrNo = fileno(stderr);
stdoutNo = fileno(stdout);
setbuf(logFile, NULL);
/* pipe both stderr and stdout to our logfile. */
dup2(fileNo, stderrNo);
dup2(fileNo,stdoutNo);

/* little debugging info */
host = slamGetHost();
warn("Host is: %s", host);
warn("creating %s", target->chrom);

/* create fasta files and run slam.pl on them. */
createFastaFilesForBits(chrNibDir, target, FALSE); 
warn("creating %s", aligns->chrom); 
createFastaFilesForBits(alignNibDir, aligns, TRUE);
warn("running slam"); 
runSlamExe(outputDir,target, aligns, refPrefix, alignPrefix);
warn("removing stuff"); 

/* Cleanup. */
carefulClose(&logFile);
chdir(cwd);
removeTempDir(target, aligns);
genomeBitFreeList(&target);
genomeBitFreeList(&aligns);
}

int main(int argc, char *argv[])
{
if(argc < 7)
    usage();
bpLimit = atoi(argv[1]);
runSlam(argv[2], argv[3], argv[4], argv[5], argv[6], argv[9], argv+10, argc-10);
return 0;
}
