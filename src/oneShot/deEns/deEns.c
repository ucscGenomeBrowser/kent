/* deEns - reformat an Ensemble bac with contigs to our style... */
#include "common.h"
#include "fa.h"

void backupName(char *name, char backup[512])
/* Convert something.x to something.bak. */
{
char dir[256],file[128],extension[64];
splitPath(name, dir, file, extension);
sprintf(backup, "%s%s%s", dir, file, ".bak");
uglyf("%s -> %s\n", name, backup);
}

int main(int argc, char *argv[])
{
char bakName[256];
char contigName[256];
char dir[256],acc[128],extension[64];
char *name;
struct dnaSeq *seqList, *seq;
int contigIx;
int i;
FILE *f;

if (argc < 2)
    errAbort("deEns file(s).fa\n");
for (i=1; i<argc; ++i)
    {
    name = argv[i];
    printf("Processing %s\n", name);
    backupName(name, bakName);
    if (rename(name, bakName) < 0)
	errAbort("Couldn't rename %s to %s\n", name, bakName);
    seqList = faReadAllDna(bakName);
    f = mustOpen(name, "w");
    splitPath(name, dir, acc, extension);
    contigIx = 1;
    for (seq = seqList; seq != NULL; seq = seq->next)
	{
	sprintf(contigName, "%s.%d", acc, contigIx);
	tolowers(seq->dna);
	faWriteNext(f, contigName, seq->dna, seq->size);
	++contigIx;
	}
    freeDnaSeqList(&seqList);
    fclose(f);
    }
}
