/* mafOrder - Filter out maf files. */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "chain.h"
#include "options.h"
#include "maf.h"
#include "bed.h"

static char const rcsid[] = "$Id: mafOrder.c,v 1.3 2006/08/14 15:41:20 angie Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafOrder - order components within a maf file\n"
  "usage:\n"
  "   mafOrder mafIn order.lst mafOut\n"
  "where order.lst has one species per line\n"
  "options:\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct mafComp *compHolders[100];

void mafOrder(char *mafIn, char *listFile,  char *mafOut)
/* mafOrder - order the components in a maf file */
{
FILE *f = mustOpen(mafOut, "w");
struct lineFile *lf = lineFileOpen(listFile, TRUE);
struct mafFile *mf = mafOpen(mafIn);
struct mafAli *maf;
char *row[1];
struct slName *elemList = NULL, *elem;

mafWriteStart(f, mf->scoring);

while (lineFileRow(lf, row))
    slNameStore(&elemList, row[0]);

lineFileClose(&lf);
slReverse(&elemList);

while((maf = mafNext(mf)) != NULL)
    {
    int numComps = 0;
    int ii;
    struct mafComp *comp;

    for(elem = elemList; elem ; elem = elem->next)
	{
	if ((comp = mafMayFindComponentDb(maf, elem->name)) != NULL)
	    {
	    compHolders[numComps++] = comp;
	    }
	}
     
    maf->components = NULL;
    for(ii=0; ii < numComps; ii++)
	slAddTail(&maf->components, compHolders[ii]);

    mafWrite(f, maf);
    mafAliFree(&maf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

mafOrder(argv[1], argv[2], argv[3]);
return 0;
}
