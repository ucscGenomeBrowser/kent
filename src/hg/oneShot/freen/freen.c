/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "htmlPage.h"
#include "sqlList.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen fileName\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void freen(char *string)
{
uglyf("%d ',' in %s\n", countChars(string, ','), string);
int count = chopByChar(cloneString(string), ',', NULL, 0);
uglyf("Count by chopByChar is %d\n", count);
char **array;
int arraySize;
sqlStringDynamicArray(cloneString(string), &array, &arraySize);
uglyf("sqlStringDynamicArray yields %d\n", arraySize);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
