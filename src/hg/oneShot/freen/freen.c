/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "obscure.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen count");
}

struct numList
/* A list of numbers. */
    {
    struct numList *next;
    int num;
    };

void freen(char *countString)
/* Print status code. */
{
int count = atoi(countString);
int i;
struct numList *list = NULL, *num;

for (i=0; i<count; ++i)
    {
    AllocVar(num);
    num->num = i;
    slAddHead(&list, num);
    }
shuffleList(&list);
for (num=list; num != NULL; num = num->next)
    printf("%d\n", num->num);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
}
