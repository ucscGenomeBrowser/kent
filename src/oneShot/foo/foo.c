/* foo.c - Some temporary thing. */
#include "common.h"
#include "fof.h"
#include "keys.h"


int main(int argc, char *argv[])
{
char *listName = "finished.new";
char *fofName = "unfinished_acc.fof";
char *outName = "frags.txt";
FILE *out = mustOpen(outName, "w");
struct fof *fof = fofOpen(fofName, NULL);
struct kvt *kvt = newKvt(64);
char line[512];
int lineCount = 0;
FILE *lf;
char *keyText;
char *s, *t;
int size;

lf = mustOpen(listName, "r");
while (fgets(line, sizeof(line), lf))
    {
    ++lineCount;
    kvtClear(kvt);
    s = trimSpaces(line);    
    t = strchr(s, '.');
    if (t != NULL)
        *t = 0;
    keyText = fofFetchString(fof, s, &size);
    kvtParseAdd(kvt, keyText);
    fprintf(out, "%s phase %s frags %s\n", s, kvtLookup(kvt, "pha"), kvtLookup(kvt, "frg"));
    freez(&keyText);
    }
freeKvt(&kvt);
return 0;
}