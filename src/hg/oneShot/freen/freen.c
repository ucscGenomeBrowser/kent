/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "dnautil.h"
#include "jksql.h"
#include "visiGene.h"

static char const rcsid[] = "$Id: freen.c,v 1.57 2005/10/26 20:15:51 kent Exp $";

void usage()
{
errAbort("freen - test some hair brained thing.\n"
         "usage:  freen now\n");
}

void freen(char *fileName)
/* Test some hair-brained thing. */
{
struct sqlConnection *conn = sqlConnect("visiGene");
struct slInt *gene, *geneList = sqlQuickNumList(conn,
	"select id from gene limit 10");
struct slName *list, *el;
printf("got %d genes\n", slCount(geneList));
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    char *name = vgGeneNameFromId(conn, gene->val);
    printf("id %d, name %s\n", gene->val, name);
    freeMem(name);
    }

list = visiGeneGeneName(conn, 16337);
for (el = list; el != NULL; el = el->next)
    printf(" %s", el->name);
printf("\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
