/* axtQueryCount - Count bases covered on each query sequence. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtQueryCount - Count bases covered on each query sequence\n"
  "usage:\n"
  "   axtQueryCount in.axt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct qInfo
/* Info on one query */
    {
    struct qInfo *next;
    char *name;	/* Name - allocated in hash. */
    int covered;  /* Bases covered. */
    };

int qInfoCmpName(const void *va, const void *vb)
/* Compare to sort based on name. */
{
const struct qInfo *a = *((struct qInfo **)va);
const struct qInfo *b = *((struct qInfo **)vb);
return strcmp(a->name, b->name);
}

void axtQueryCount(char *fileName)
/* axtQueryCount - Count bases covered on each query sequence. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
struct axt *axt;
struct qInfo *qList = NULL, *q;

while ((axt = axtRead(lf)) != NULL)
    {
    char *qName = axt->qName;
    if ((q = hashFindVal(hash, qName)) == NULL)
        {
	AllocVar(q);
	slAddHead(&qList, q);
	hashAddSaveName(hash, qName, q, &q->name);
	}
    q->covered += axt->qEnd - axt->qStart;
    axtFree(&axt);
    }
slSort(&qList, qInfoCmpName);
for (q = qList; q != NULL; q = q->next)
    printf("%s\t%d\n", q->name, q->covered);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtQueryCount(argv[1]);
return 0;
}
