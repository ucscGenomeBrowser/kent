/* rmskOoJobs - make Condor repeat mask o+o sequence contigs. */
#include "common.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "rmskOoJobs - make Condor repeat mask o+o sequence contigs\n"
  "usage:\n"
  "   rmskOoJobs ooDir conDir\n"
  "This will create conDir and fill it up with a condor submission file\n"
  "and various log files.  Please give full path names to both ooDir and conDir\n");
}

struct fileInfo *getAllCtgFa(char *ooDir)
/* Get list of all fa files in all contigs. */
{
struct fileInfo *chromList, *chrom;
struct slName *contigList, *contig;
long size;
char faName[512];
char dirName[512];
struct fileInfo *fiList = NULL, *fi;

chromList = listDirX(ooDir, "*", FALSE);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (chrom->isDir && strlen(chrom->name) <= 2)
        {
	sprintf(dirName, "%s/%s", ooDir, chrom->name);
	contigList = listDir(dirName, "ctg*");
	for (contig = contigList; contig != NULL; contig = contig->next)
	    {
	    sprintf(faName, "%s/%s/%s/%s.fa", ooDir, chrom->name, contig->name, contig->name);
	    size = fileSize(faName);
	    if (size > 0)
	        {
		fi = newFileInfo(faName, size, FALSE, FALSE);
		slAddHead(&fiList, fi);
		}
	    }
	}
    }
slReverse(&fiList);
return fiList;
}

int cmpFileInfoSize(const void *va, const void *vb)
/* Compare two fileInfo by size for sorting with biggest first. */
{
const struct fileInfo *a = *((struct fileInfo **)va);
const struct fileInfo *b = *((struct fileInfo **)vb);
return b->size - a->size;
}

void rmskOoJobs(char *ooDir, char *conDir)
/* rmskOoJobs - make Condor repeat mask o+o sequence contigs. */
{
char conFile[512];
char conOutDir[512];
char conErrDir[512];
char conLogDir[512];
FILE *con;
struct fileInfo *fiList = NULL, *fi;
char dir[256], file[128], ext[64], path[512];

/* Set up basic directory structure in output dir. */
makeDir(conDir);
sprintf(conOutDir, "%s/out", conDir);
makeDir(conOutDir);
sprintf(conLogDir, "%s/log", conDir);
makeDir(conLogDir);
sprintf(conErrDir, "%s/err", conDir);
makeDir(conErrDir);

/* Create condor submission file and write header. */
sprintf(conFile, "%s/all.con", conDir);
con = mustOpen(conFile, "w");
fprintf(con, "#File created by rmskOoJobs %s %s\n\n", ooDir, conDir);
fprintf(con,
"universe        = vanilla\n"
"notification    = error\n"
"requirements    = memory > 250\n"
"executable      = /cse/guests/kent/bin/rmLocal.sh\n"
"\n");

/* Get list of files to do, sorted by size. */
fiList = getAllCtgFa(ooDir);
printf("Got %d contig fa files\n", slCount(fiList));
slSort(&fiList, cmpFileInfoSize);
printf("Largest is %ld bytes\n", fiList->size);

/* Make a job for each contig. */
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    splitPath(fi->name, dir, file, ext);
    dir[strlen(dir)-1] = 0;	/* chop off final '/' */
    fprintf(con, "initialdir = %s\n", dir);
    fprintf(con, "arguments = %s%s\n", file, ext);
    fprintf(con, "log = %s/%s\n", conLogDir, file);
    fprintf(con, "error = %s/%s\n", conErrDir, file);
    fprintf(con, "output = %s/%s\n", conOutDir, file);
    fprintf(con, "queue 1\n");
    fprintf(con, "\n");
    }

fclose(con);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
rmskOoJobs(argv[1], argv[2]);
return 0;
}
