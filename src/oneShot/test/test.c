/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "memalloc.h"
#include "rbTree.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test in out\n"
  "options:\n"
  );
}


void dumpString(void *item, FILE *f)
{
fprintf(f, "%s", (char *)item);
}

void test(char *in, char *out)
/* test - Test something. */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f = mustOpen(out, "w");
char *line, *word;
struct rbTree *rb = rbTreeNew(rbTreeCmpString);
struct slRef *list, *ref;
while (lineFileNext(lf, &line, NULL))
    {
    while ((word = nextWord(&line)) != NULL)
        {
	word = cloneString(word);
	rbTreeAdd(rb, word);
	}
    }
rbTreeDump(rb, f, dumpString);
list = rbTreeItems(rb);
for (ref = list; ref != NULL; ref = ref->next)
    printf("%s\n", (char *)(ref->val));
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
   usage();
test(argv[1], argv[2]);
return 0;
}
