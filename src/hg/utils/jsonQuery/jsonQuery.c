/* jsonQuery - Use a path syntax to retrieve elements/values from each line of JSON input. */
#include "common.h"
#include "jsHelper.h"
#include "jsonQuery.h"
#include "linefile.h"
#include "net.h"
#include "obscure.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "jsonQuery - Use a path syntax to retrieve elements/values from each line of JSON input\n"
  "usage:\n"
  "   jsonQuery input.json path output.js\n"
  "options:\n"
  "   -uniq        Print out unique values as they are found (instead of all values)\n"
  "   -countUniq   Print out unique values and the number of times each occurs at end of input\n"
  "\n"
  "Except for -uniq and -countUniq modes, objects and lists in output are pretty-printed\n"
  "with newlines, so they are JavaScript values not JSON.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    { "uniq",         OPTION_BOOLEAN },
    { "countUniq",    OPTION_BOOLEAN },
    { NULL, 0 },
};

void jsonQuery(char *inFile, char *path, char *outFile)
/* jsonQuery - Use a path syntax to retrieve elements/values from each line of JSON input. */
{
struct lineFile *lf = netLineFileOpen(inFile);
struct hash *uniqHash = NULL;
boolean countUniq = optionExists("countUniq");
boolean uniq = optionExists("uniq") || countUniq;
if (uniq)
    uniqHash = hashNew(0);
struct dyString *dy = dyStringNew(0);
FILE *outF = mustOpen(outFile, "w");
char *line;
while (lineFileNextReal(lf, &line))
    {
    struct lm *lm = lmInit(1<<16);
    struct jsonElement *topEl = jsonParseLm(line, lm);
    struct slRef topRef;
    topRef.next = NULL;
    topRef.val = topEl;
    char desc[1024];
    safef(desc, sizeof desc, "line %d of %s", lf->lineIx, inFile);
    struct slRef *results = jsonQueryElementList(&topRef, desc, path, lm);
    struct slRef *result;
    for (result = results;  result != NULL;  result = result->next)
        {
        struct jsonElement *el = result->val;
        if (uniq)
            {
            dyStringClear(dy);
            jsonDyStringPrint(dy, el, NULL, -1);
            char *elStr = dy->string;
            int count = hashIntValDefault(uniqHash, elStr, 0);
            if (count < 1)
                {
                hashAddInt(uniqHash, elStr, 1);
                verbose(2, "line %d: %s\n", lf->lineIx, elStr);
                if (!countUniq)
                    {
                    fprintf(outF, "%s\n", elStr);
                    fflush(outF);
                    }
                }
            else
                hashIncInt(uniqHash, elStr);
            }
        else
            jsonPrintToFile(el, NULL, outF, 2);
        }
    lmCleanup(&lm);
    }
lineFileClose(&lf);
if (countUniq)
    {
    struct hashEl *hel;
    struct hashCookie cookie = hashFirst(uniqHash);
    while ((hel = hashNext(&cookie)) != NULL)
        {
        fprintf(outF, "%10d %s\n", ptToInt(hel->val), hel->name);
        }
    }
carefulClose(&outF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
jsonQuery(argv[1], argv[2], argv[3]);
return 0;
}
