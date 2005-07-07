/* vgLoadJax - Load visiGene database from jackson database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgLoadJax - Load visiGene database from jackson database\n"
  "usage:\n"
  "   vgLoadJax jaxDb visiDb\n"
  "Load everything in jackson database tagged after date to\n"
  "visiGene database.  Most commonly run as\n"
  "   vgLoadJax jackson visiGene\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void vgLoadJax(char *jaxDb, char *visiDb)
/* vgLoadJax - Load visiGene database from jackson database. */
{
struct sqlConnection *jc = sqlConnect(jaxDb);
struct sqlConnection *vc = sqlConnect(visiDb);

uglyf("vgLoadJax %s %s\n", jaxDb, visiDb);

sqlDisconnect(&vc);
sqlDisconnect(&jc);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
vgLoadJax(argv[1], argv[2]);
return 0;
}
