/* orderMiddle - Given a list of items and distance between items, order list by a greedy method
 * to order in such a way to minimize distance between adjacent items.  */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
#include "rainbow.h"
#include "sqlNum.h"
#include "pairDistance.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "orderMiddle - Given a list of items and distance between items, order list by a greedy\n"
  "method to order in such a way to minimize distance between adjacent items.\n"
  "usage:\n"
  "   orderMiddle in.lst in.pairs out.lst\n"
  "where in.lst has one item per line, and out.lst does as well\n"
  "in.pairs is whitespace separated with the columns <a> <b> <distance>\n"
  "where a and b are items from in.lst\n"
  "options:\n"
  "   -invert - deal with cases such as correlation where high numbers indicate proximity\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"invert", OPTION_BOOLEAN},
   {NULL, 0},
};


struct dlNode *findClosest(struct dlList *list, struct hash *pairHash, struct dlNode *xNode)
/* Find closest node in list to x. */
{
struct slName *x = xNode->val;
char *xName = x->name;
double closest = BIGDOUBLE;
struct dlNode *closestNode = NULL, *node;
for (node = list->head; !dlEnd(node); node = node->next)
    {
    struct slName *item = node->val;
    double distance = pairDistanceHashLookup(pairHash, xName, item->name);
    if (distance <= closest)
         {
	 closest = distance;
	 closestNode = node;
	 }
    }
assert(closestNode != NULL);
return closestNode;
}

struct dlList *greedyOrder(struct slName *itemList, struct hash *pairHash)
/* Return a list of references to items, which includes all items, but reordered
 * so as to have close items relatively near each other.  This actually will
 * preserve the start and end of the list though. */
{
/* Put all items in todo list */
struct dlList *toDo = dlListNew();
struct slName *item;
int itemCount = 0;
for (item = itemList; item != NULL; item = item->next)
     {
     dlAddValTail(toDo, item);
     ++itemCount;
     }

/* For short lists, they are already in order */
if (itemCount <= 2)
    return toDo;

/* Initialize start and end lists */
struct dlNode *head = dlPopHead(toDo);
struct dlList *startOrder = newDlList();
dlAddTail(startOrder, head);
struct dlNode *tail = dlPopTail(toDo);
struct dlList *endOrder = newDlList();
dlAddHead(endOrder, tail);

/* Main loop - first find closest thing remaining to start and add it to start list 
 * then do same on tail list */
for (;;)
    {
    /* Do start */
    if (dlEmpty(toDo))
         break;
    struct dlNode *closest = findClosest(toDo, pairHash, head);
    dlRemove(closest);
    dlAddTail(startOrder, closest);
    head = closest;

    /* Do end */
    if (dlEmpty(toDo))
         break;
    closest = findClosest(toDo, pairHash, tail);
    dlRemove(closest);
    dlAddHead(endOrder, closest);
    tail = closest;
    }

/* Concatenate start and end */
dlCat(startOrder, endOrder);

/* Clean up and return */
dlListFree(&toDo);
dlListFree(&endOrder);
return startOrder;
}

void orderMiddle(char *inList, char *inPairs, char *outTab)
/* orderMiddle - Given a list of items and distance between items, order list by a greedy method
 * to order in such a way to minimize distance between adjacent items.  */
{
struct slName *itemList = readAllLines(inList);
verbose(2, "%d items in %s\n", slCount(itemList), inList);
struct pairDistance *pairList= pairDistanceReadAll(inPairs);
if (optionExists("invert"))
    pairDistanceInvert(pairList);
verbose(2, "%d pairs in %s\n", slCount(pairList), inPairs);
struct hash *pairHash = pairDistanceHashList(pairList);
struct dlList *orderedList = greedyOrder(itemList, pairHash);
struct dlNode *node;
FILE *f = mustOpen(outTab, "w");
for (node = orderedList->head; !dlEnd(node); node = node->next)
    {
    struct slName *item = node->val;
    fprintf(f, "%s\n", item->name);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
orderMiddle(argv[1], argv[2], argv[3]);
return 0;
}
