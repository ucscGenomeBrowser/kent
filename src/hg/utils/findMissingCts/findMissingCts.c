/* findMissingCts - scan a dump of namedSessionDb for missing custom track bed files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "namedSessionDb.h"
#include "cheapcgi.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "findMissingCts - scan a dump of namedSessionDb for missing custom track bed files\n"
  "usage:\n"
  "   findMissingCts namedSessionDump.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *otherFile(char *fileName)
{
char *directory = cloneString(fileName);
char *lastSlash = strrchr(directory, '/');

if (lastSlash == NULL)
    return "NoPath"; //errAbort("expect at most one slash in missing filenames %s", directory);

*lastSlash = 0;

if (!fileExists(directory))
    return "NoDirectory";

struct fileInfo *info = listDirXExt(directory, "ct*.bed", TRUE, TRUE);

if (info == NULL)
    return "NoFile";

struct fileInfo *most = info;
info = info->next;
for(; info; info = info->next)
    if (info->creationTime > most->creationTime)
        most = info;

return most->name;
}

void findCtFile(struct namedSessionDb *session)
{
char *contentPtr = session->contents;
unsigned contentLength = strlen(contentPtr);
char *endOfContents = &contentPtr[contentLength];
char *ctfilePtr;
while((contentPtr != endOfContents) && (ctfilePtr = strstr(contentPtr, "ctfile")))
    {
    char *equals = strchr(ctfilePtr, '=');
    if (equals == NULL)
        errAbort("didn't find equals after ctfile %s\n", ctfilePtr);

    char *end = strchr(equals, '&');
    if (end == NULL)
        end = endOfContents;
    else
        *end = 0;

    if ((contentPtr == ctfilePtr) || (ctfilePtr[-1] == '&'))
        {
        *equals = 0;
        equals++;

        char fileName[4096];
        cgiDecode(equals, fileName, end - equals);

            //printf("%s %s %s %s\n", session->userName, session->sessionName, ctfilePtr, fileName);
        if (!fileExists(fileName))
            printf("%s\t%s\t%s\t%s\t%s\t%s\n", session->userName, session->sessionName, session->lastUse, ctfilePtr, fileName, otherFile(fileName));


        }
    contentPtr = end;
    }
}

void findMissingCts(char *namedSessionDump)
/* findMissingCts - scan a dump of namedSessionDb for missing custom track bed files. */
{
struct lineFile *lf = lineFileOpen(namedSessionDump, TRUE);
struct namedSessionDb aSession;

char *words[NAMEDSESSIONDB_NUM_COLS];
while(lineFileRowTab(lf, words))
    {
    namedSessionDbStaticLoad(words, &aSession);
    findCtFile(&aSession);
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
findMissingCts(argv[1]);
return 0;
}
