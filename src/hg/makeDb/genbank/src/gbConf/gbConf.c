/* gbConf -  print contents of genbank.conf file for debugging purposes */
#include "common.h"
#include "options.h"
#include "hash.h"
#include "gbConf.h"

static char const rcsid[] = "$Id: gbConf.c,v 1.1 2005/08/27 07:46:59 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n\n"
         "gbConf genebank.conf\n"
         "\n"
         "Print contents of genbank.conf with variables expanded for debugging purposes.\n"
         "The results will not contain comments and will be alphabetically sorted\n"
         "by item name.\n",
         msg);
}

void printGbConf(FILE *fh, char *confFile)
/* Print contents of genbank.conf file for debugging purposes */
{
struct gbConf *conf = gbConfNew(confFile);
struct hashEl *elems = hashElListHash(conf->hash);
struct hashEl *elem;

slSort(&elems, hashElCmp);

for (elem = elems; elem != NULL; elem = elem->next)
    fprintf(fh, "%s = %s\n", elem->name, (char*)elem->val);

hashElFreeList(&elems);
gbConfFree(&conf);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 2)
    usage("wrong # args");

printGbConf(stdout, argv[1]);

return 0;
}



/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
