/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "htmlPage.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen fileName\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct htmlTag *findNextMatching(struct htmlTag *list, char *name)
/* Return first tag in list that is of type name or NULL if not found*/
{
struct htmlTag *tag;
for (tag = list; tag != NULL; tag = tag->next)
   if (sameWord(name, tag->name))
       return tag;
return NULL;
}

void freen(char *url)
{
struct htmlPage *page = htmlPageGet(url);
printf("%s\n", page->htmlText);
htmlPageFree(&page);
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
