/* randomLines - Pick out random lines from file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "randomLines - Pick out random lines from file\n"
  "usage:\n"
  "   randomLines inFile count outFile\n"
  "options:\n"
  "   -decomment - remove blank lines and those starting with \n"
  );
}

boolean decomment = FALSE;

struct dlList *readLines(char *fileName)
/* Read all lines into a dlList. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
struct dlList *list = newDlList();
while (lineFileNext(lf, &line, NULL))
    {
    if (decomment)
        {
	char *s = skipLeadingSpaces(line);
	if (s[0] == 0 || s[0] == '#')
	    continue;
	}
    line = cloneString(line);
    dlAddValTail(list, line);
    }
return list;
}

struct dlNode *dlNodeForIx(struct dlList *list, int ix)
/* Return node at given position on list. */
{
int i;
struct dlNode *node = list->head;
for (i=0; i<ix; ++i)
    {
    if (dlEnd(node))
        errAbort("Not %d items on list", ix);
    node = node->next;
    }
return node;
}

void randomLines(char *inName, int count, char *outName)
/* randomLines - Pick out random lines from file. */
{
FILE *f = mustOpen(outName, "w");
struct dlList *list = readLines(inName);
struct dlNode *node;
char *s;
int lineCount = dlCount(list);
int ix, randomIx;
if (lineCount < count)
    {
    warn("Only %d lines in %s", lineCount, inName);
    count = lineCount;
    }
for (ix = count; ix>0; --ix)
    {
    randomIx = rand()%lineCount;
    node = dlNodeForIx(list, randomIx);
    dlRemove(node);
    s = node->val;
    fprintf(f, "%s\n", s);
    lineCount -= 1;
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4 || !isdigit(argv[2][0]))
    usage();
decomment = optionExists("decomment");
randomLines(argv[1], atoi(argv[2]), argv[3]);
return 0;
}
