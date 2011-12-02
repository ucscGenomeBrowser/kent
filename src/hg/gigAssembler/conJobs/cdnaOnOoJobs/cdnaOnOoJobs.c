/* cdnaOnOoJobs - make condor submission file for EST and mRNA alignments on draft assembly. */
#include "common.h"
#include "portable.h"
#include "hash.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdnaOnOoJobs - make condor submission file for EST and mRNA alignments on draft assembly\n"
  "usage:\n"
  "   cdnaOnOoJobs ooDir conDir cdna(s)\n"
  "This will create conDir and fill it up with a condor submission file\n"
  "and various log files.  Please give full path names to both ooDir and conDir\n"
  "cdna should be 'refseq', 'mrna', and/or 'est'");
}

void makeSimpleIn(char *inDir, char *inName, char *faName)
/* Create file inDir/inName containing faName. */
{
char inPath[512];
FILE *f;

sprintf(inPath, "%s/%s", inDir, inName);
f = mustOpen(inPath, "w");
fprintf(f, "%s\n", faName);
fclose(f);
}

void doFullJob(FILE *con, char *chromFile, char *chromName,
    char *conDir, char *pslDir, char *outDir, char *logDir,
    char *errDir, char *inDir, char *cdna)
/* Do mrna alignment job on full chromosome. */
{
printf("Full %s job on %s\n", cdna, chromFile);

fprintf(con, "log = %s/%s.%s\n", logDir, chromName, cdna);
fprintf(con, "error = %s/%s.%s\n", errDir, chromName, cdna);
fprintf(con, "output = %s/%s.%s\n", outDir, chromName, cdna);
makeSimpleIn(inDir, chromName, chromFile);
fprintf(con, 
    "arguments = %s/%s %s/%s mrna /var/tmp/hg/h/10.ooc %s/%s.%s.psl\n",
    inDir, chromName, inDir, cdna, pslDir, chromName, cdna);
fprintf(con, "queue 1\n\n");
}


void doPieceJob(FILE *con, FILE *sh, char *ooDir, char *chrom, char *cdna,
    char *conDir, char *pslDir, char *outDir, char *logDir, char *errDir, char *inDir)
/* Do mrna or EST alignment jobs on contigs. */
{
struct fileInfo *fileList, *fel;
char chromDir[512];
char pslSubDir[512];
char contigDir[512];
char faName[512];
char jobName[512];

printf("Piece job on %s %s\n", chrom, cdna);
fprintf(sh, "#Stitching %s pieces for chromosome %s\n", cdna, chrom);

/* Create a directory for result from each contig. */
sprintf(pslSubDir, "%s/%s", pslDir, chrom);
makeDir(pslSubDir);

/* List each contig and make a job for it. */
sprintf(chromDir, "%s/%s", ooDir, chrom);
fileList = listDirX(chromDir, "ctg*", FALSE);
for (fel = fileList; fel != NULL; fel = fel->next)
    {
    char *contig = fel->name;
    sprintf(contigDir, "%s/%s/%s", ooDir, chrom, contig);
    sprintf(faName, "%s/%s.fa", contigDir, contig);
    sprintf(jobName, "%s_%s", chrom, contig);
    if (fileExists(faName))
	{
	fprintf(con, "log = %s/%s.%s\n", logDir, jobName, cdna);
	fprintf(con, "error = %s/%s.%s\n", errDir, jobName, cdna);
	fprintf(con, "output = %s/%s.%s\n", outDir, jobName, cdna);
	makeSimpleIn(inDir, jobName, faName);
	fprintf(con, 
	    "arguments = %s/%s %s/%s mrna /var/tmp/hg/h/10.ooc %s/%s.%s.psl\n",
	    inDir, jobName, inDir, cdna, pslSubDir, contig, cdna);
	fprintf(con, "queue 1\n\n");
	}
    }
}

void chopDotsAndUnderbars(const char *s, char *d)
/* Remove everything past _ or . */
{
strcpy(d, s);
chopSuffix(d);
if ((d = strchr(d, '_')) != NULL)
    *d = 0;
}

boolean sameChrom(char *a, char *b)
/* Return TRUE if file names a, b refer to same chromosome. */
{
char aBuf[512], bBuf[512];
chopDotsAndUnderbars(a, aBuf);
chopDotsAndUnderbars(b, bBuf);
return sameString(aBuf, bBuf);
}

int cmpFileInfoUnderbar(const void *va, const void *vb)
/* Compare two fileInfo. */
{
const struct fileInfo *a = *((struct fileInfo **)va);
const struct fileInfo *b = *((struct fileInfo **)vb);
char aBuf[512], bBuf[512];
int diff;
chopDotsAndUnderbars(a->name, aBuf);
chopDotsAndUnderbars(b->name, bBuf);
diff = strcmp(aBuf, bBuf);
if (diff == 0)
    diff = strcmp(a->name,b->name);
return diff;
}

void cdnaOnOoJobs(char *ooDir, char *conDir, int cdnaCount, char *cdnaTypes[])
/* cdnaOnOoJobs - make condor submission file for EST and mRNA alignments on draft assembly. */
{
char chromDir[512];
char chromFile[512];
char conFile[512];
char shFile[512];
char conPslDir[512];
char conOutDir[512];
char conErrDir[512];
char conLogDir[512];
char conInDir[512];
struct fileInfo *cfaList, *cfa;
struct fileInfo *chromList, *chromEl;
static char lastChromName[64] = "9X8Y";	/* Something uniq. */
boolean lastDoFull = FALSE;
FILE *con, *sh;
int i;


/* Set up basic directory structure in output dir. */
makeDir(conDir);
sprintf(conPslDir, "%s/psl", conDir);
makeDir(conPslDir);
sprintf(conOutDir, "%s/out", conDir);
makeDir(conOutDir);
sprintf(conLogDir, "%s/log", conDir);
makeDir(conLogDir);
sprintf(conErrDir, "%s/err", conDir);
makeDir(conErrDir);
sprintf(conInDir, "%s/in", conDir);
makeDir(conInDir);

/* Create list files for mrna and est. */
for (i=0; i<cdnaCount; i++)
    {
    char fileName[512];
    sprintf(fileName, "/var/tmp/hg/h/mrna/%s.fa", cdnaTypes[i]);
    makeSimpleIn(conInDir, cdnaTypes[i], fileName);
    }

/* Create condor submission file and write header. */
sprintf(conFile, "%s/all.con", conDir);
con = mustOpen(conFile, "w");
fprintf(con, "#File created by cdnaOnOoJobs %s %s\n\n", ooDir, conDir);
fprintf(con,
"universe        = vanilla\n"
"notification    = error\n"
"requirements    = memory > 250\n"
"executable      = /cse/guests/kent/bin/i386/psLayout\n"
"initialdir      = %s\n"
"\n"
 , conDir);

/* Create shell script to finish job. */
sprintf(shFile, "%s/finish.sh", conDir);
sh = mustOpen(shFile, "w");

/* Loop through each chromosome directory. */
chromList = listDirX(ooDir, "*", FALSE);
for (chromEl = chromList; chromEl != NULL; chromEl = chromEl->next)
    {
    int len = strlen(chromEl->name);
    if (chromEl->isDir && len <= 2)
        {
	sprintf(chromDir, "%s/%s", ooDir, chromEl->name);
	cfaList = listDirX(chromDir, "chr*.fa", FALSE);
	slSort(&cfaList, cmpFileInfoUnderbar);

	/* Get list of assembled chromosomes in dir. */
	for (cfa = cfaList; cfa != NULL; cfa = cfa->next)
	    {
	    /* See if is _random version of previous chromosome, in which
	     * case we follow the lead of last time. */
	    printf("%s size %d\n", cfa->name, cfa->size);
	    sprintf(chromFile, "%s/%s", chromDir, cfa->name);
	    if (sameChrom(lastChromName, cfa->name))
		{
		if (lastDoFull)
		    {
		    for (i=0; i<cdnaCount; ++i)
			doFullJob(con, chromFile, lastChromName, conDir, conPslDir, 
				conOutDir, conLogDir, conErrDir, conInDir, cdnaTypes[i]);
		    }
		}
	    else
		{
		strcpy(lastChromName, cfa->name);
		chopSuffix(lastChromName);
		if (cfa->size < 60000000)
		    {
		    lastDoFull = TRUE;
		    for (i=0; i<cdnaCount; ++i)
			doFullJob(con, chromFile, lastChromName, conDir, conPslDir, conOutDir, 
			    conLogDir, conErrDir, conInDir, cdnaTypes[i]);
		    }
		else
		    {
		    lastDoFull = FALSE;
		    for (i=0; i<cdnaCount; ++i)
			doPieceJob(con, sh, ooDir, lastChromName+3, cdnaTypes[i], 
			    conDir, conPslDir, conOutDir, conLogDir, conErrDir, conInDir);
		    }
		}
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 4)
    usage();
cdnaOnOoJobs(argv[1], argv[2], argc-3, argv+3);
return 0;
}
