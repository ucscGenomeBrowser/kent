/* cartEditSuperTrack - make a cart edit to put tracks into a supertrack. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cartEditSuperTrack - make a cart edit to put tracks into a supertrack\n"
  "usage:\n"
  "   cartEditSuperTrack tracks.txt cartVersion# outputFile\n"
  "where tracks.txt is a tab-separated two column file with a list of tracks and the supertrack they're moving to.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static void writeFile(FILE *f, struct hash *superHash, char *version)
{
struct dyString *varDeclare = dyStringNew(4096);
struct dyString *editFunc = dyStringNew(4096);

dyStringPrintf(varDeclare,
"#include \"common.h\"\n"
"#include \"cart.h\"\n"
"\n");

dyStringPrintf(editFunc,
"void cartEdit%s(struct cart *cart)\n"
"{\n", version);

struct hashCookie cookie = hashFirst(superHash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    char name[4096];

    safef(name, sizeof name, "edit%s%sTracks", version, (char *)hel->name);

    dyStringPrintf(editFunc, "cartTurnOnSuper(cart, %s,  ArraySize(%s),  \"%s\");\n", name, name, hel->name);

    dyStringPrintf(varDeclare, "static char *%s[] =\n{\n", name);

    struct hashCookie cookie2 = hashFirst((struct hash *)hel->val);
    struct hashEl *hel2;
    while ((hel2 = hashNext(&cookie2)) != NULL)
        {
        dyStringPrintf(varDeclare, "\"%s\",\n", hel2->name);
        }
    dyStringPrintf(varDeclare, "};\n\n");
    }
dyStringPrintf(editFunc, "}\n");
fputs(varDeclare->string, f);
fputs(editFunc->string, f);
fclose(f);
}

void cartEditSuperTrack(char *input, char *versionNumber, char *outFile)
/* cartEditSuperTrack - make a cart edit to put tracks into a supertrack. */
{
FILE *f = mustOpen(outFile, "w");
struct lineFile *lf = lineFileOpen(input, TRUE);
char *row[2];
struct hash *superHash = newHash(5);

while (lineFileRow(lf, row))
    {
    struct hash *trackHash = newHash(5);
    struct hashEl *hel;

    if ((hel = hashLookup(superHash, row[1])) == NULL)
        {
        trackHash = newHash(5);
        hashAdd(superHash, row[1], trackHash);
        }
    else
        trackHash = hel->val;

    hashStore(trackHash, row[0]);
    }

writeFile(f, superHash, versionNumber);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
cartEditSuperTrack(argv[1], argv[2], argv[3]);
return 0;
}
