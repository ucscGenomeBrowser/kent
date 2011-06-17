/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "jksql.h"
#include "trackDb.h"
#include "hui.h"
#include "correlate.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

void freen(char *input)
/* Test some hair-brained thing. */
{
struct hash *hash = raReadAll(input, "track"); 
struct hashEl *hohList = hashElListHash(hash);
struct hashEl *hohEl;
for (hohEl = hohList; hohEl != NULL; hohEl = hohEl->next)
    {
    struct hashEl *helList = hashElListHash(hohEl->val);
    struct hashEl *hel;
    for (hel = helList; hel != NULL; hel = hel->next)
        printf("%s: %s\n", hel->name, (char*)hel->val);
    printf("\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
