/* m22dirs - manglled chromosoem 22 test dirs. */
#include "common.h"
#include "obscure.h"
#include "portable.h"

void usage()
{
errAbort(
  "m22dirs faDir ooDir, ooDir.8\n");
}

int ctgFromName(char *name)
/* Return contig given file name of form T_Chr22ctg4_003.fa.
 * In this case would want to return 4. */
{
char *head = "T_Chr22ctg";
int headSize = strlen(head);
char *s;

if (!startsWith(head, name))
    errAbort("%s doesn't start with %s", name, head);
s = name + headSize;
if (!isdigit(s[0]))
    errAbort("%s doesn't have a number after %s\n", name, head);
return atoi(s);
}

void m22Dirs(char *faDir, char *ooDir, char *ooDir8)
{
struct slName *faDirList, *faDirEl;
char *faName;
char fileName[512];
int ctg8Ix = 0;
int ctgIx;
int modContig = 0;
int modContig8 = 0;
int eight = 8;
char ctg8Name[64];
char ctgDir[512];
char ctg8Dir[512];
char acc[128];
int lastCtgIx = -1;
FILE *geno8 = NULL, *geno = NULL;
FILE *info = NULL, *info8 = NULL;

mkdir(ooDir, 0775);
mkdir(ooDir8, 0775);
sprintf(fileName, "%s/22", ooDir);
mkdir(fileName, 0775);
sprintf(fileName, "%s/22", ooDir8);
mkdir(fileName, 0775);
faDirList = listDir(faDir, "*.fa");
slSort(&faDirList, slNameCmp);
for (faDirEl = faDirList; faDirEl != NULL; faDirEl = faDirEl->next)
    {
    faName = faDirEl->name;
    ctgIx = ctgFromName(faName);
    if (ctgIx != lastCtgIx)
	{
	carefulClose(&geno);
	carefulClose(&info);
	sprintf(ctgDir, "%s/22/ctg%d", ooDir, ctgIx);
	mkdir(ctgDir, 0775);
	sprintf(fileName, "%s/geno.lst", ctgDir);
	geno = mustOpen(fileName, "w");
	sprintf(fileName, "%s/info", ctgDir);
	info = mustOpen(fileName, "w");
	fprintf(info, "ctg%d ORDERED\n", ctgIx);
	modContig = 0;
	ctg8Ix = 0;
	}
    if (ctgIx != lastCtgIx || ++modContig8 >= eight)
	{
	++ctg8Ix;
	carefulClose(&geno8);
	carefulClose(&info8);
	sprintf(ctg8Dir, "%s/ctg_%d_%02d", ooDir8, ctgIx, ctg8Ix);
	mkdir(ctg8Dir, 0775);
	sprintf(fileName, "%s/geno", ctg8Dir);
	geno8 = mustOpen(fileName, "w");
	sprintf(fileName, "%s/info", ctg8Dir);
	info8 = mustOpen(fileName, "w");
	fprintf(info8, "ctg_%d_%02d ORDERED\n", ctgIx, ctg8Ix);
	modContig8 = 0;
	}
    fprintf(geno, "%s/%s\n", faDir, faName);
    fprintf(geno8, "%s/%s\n", faDir, faName);
    strcpy(acc, faName);
    chopSuffix(acc);
    fprintf(info, " %s %d\n", acc, modContig*100);
    fprintf(info8, " %s %d\n", acc, modContig8*100);
    lastCtgIx = ctgIx;
    }
}

int main(int argc, char *argv[])
{
if (argc != 4)
    usage();
m22Dirs(argv[1], argv[2], argv[3]);
return 0;
}

