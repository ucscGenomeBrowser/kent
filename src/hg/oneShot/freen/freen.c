/* freen - My Pet Freen. */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <limits.h>
#include <dirent.h>
#include <netdb.h>
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "portable.h"
#include "jksql.h"
#include "fa.h"

static char const rcsid[] = "$Id: freen.c,v 1.37 2003/09/08 09:02:32 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen database table");
}


void freen(char *database, char *table)
/* Test some hair-brained thing. */
{
struct sqlConnection *conn = sqlConnect(database);
boolean exists = sqlTableExists(conn, table);
printf("%s.%s %d\n", database, table, exists);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
   usage();
freen(argv[1], argv[2]);
return 0;
}
