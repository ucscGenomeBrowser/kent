/* rbTest - Test rb trees. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "rbTree.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rbTest - Test rb trees\n"
  "usage:\n"
  "   rbTest XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct pos
/* Just a position. */
    {
    int pos;
    };

int compare(const void *va, const void *vb)
/* compare function */
{
const struct pos *a = va;
const struct pos *b = vb;
return a->pos - b->pos;
}

static void dumpPos(const void *item, FILE *f)
/* Print out position. */
{
const struct pos *pos = item;
fprintf(f, "%d", pos->pos);
}
void testEasy()
/* Do a relatively easy test. */
{
static int test[] = {1,3,5,7,9,8,6,4,2,0};
int i;
struct pos *pos, *nine;
struct rbTree *tree = rbTreeNew(compare);

printf("test easy\n");
for (i=0; i<ArraySize(test); ++i)
    {
    AllocVar(pos);
    pos->pos = test[i];
    rbTreeAdd(tree, pos);
    }

AllocVar(nine);
nine->pos = 9;

rbTreeDump(tree, stdout, dumpPos);

pos = rbTreeFind(tree, nine);
printf("pos = %d\n", pos->pos);

rbTreeRemove(tree, pos);
rbTreeDump(tree, stdout, dumpPos);

rbTreeFree(&tree);
tree = NULL;
printf("\n");
}

void testHard(int count)
/* Do more thorough test. */
{
int i;
int val;
struct pos *pos, *tp;
struct rbTree *tree = rbTreeNew(compare);

printf("testHard\n");
for (i=0; i<count; ++i)
    {
    AllocVar(pos);
    pos->pos = rand()%100;
    tp = rbTreeFind(tree, pos);
    if (tp != NULL && tp->pos != pos->pos)
        errAbort("Found wrong pos %d vs %d", tp->pos, pos->pos);
    rbTreeRemove(tree, pos);
    pos->pos = rand()%100;
    rbTreeAdd(tree, pos);
    AllocVar(pos);
    pos->pos = rand()%100;
    rbTreeAdd(tree, pos);
    }
rbTreeDump(tree, stdout, dumpPos);

rbTreeFree(&tree);
}

void rbTest(int count)
/* rbTest - Test rb trees. */
{
testEasy();
testHard(count);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
rbTest(atoi(argv[1]));
return 0;
}
