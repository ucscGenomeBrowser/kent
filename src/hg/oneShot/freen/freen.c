/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "dlist.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen count");
}

struct number
/* A list of numbers. */
    {
    struct numList *next;
    int number;
    };

int numberCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct number *a = *((struct number **)va);
const struct number *b = *((struct number **)vb);
return a->number - b->number;
}


void freen(char *s)
/* Print status code. */
{
int count = atoi(s);
int i;
struct number *num;
struct dlList *list = newDlList();
struct dlNode *node;

for (i=0; i<count; ++i)
    {
    AllocVar(num);
    num->number = rand();
    dlAddValTail(list, num);
    }
dlSort(list, numberCmp);
for (node = list->head; !dlEnd(node); node = node->next)
    {
    num = node->val;
    printf("%d\n", num->number);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
}
