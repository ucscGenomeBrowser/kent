/* jsonQueryTest - Read in JSON and paths from files and print the results. */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "errCatch.h"
#include "hash.h"
#include "linefile.h"
#include "options.h"
#include "jsonQuery.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "jsonQueryTest - Read in JSON and paths from files and print the results.\n"
  "usage:\n"
  "   jsonQueryTest jsonIn.txt pathIn.txt out.txt\n"
  "jsonIn.txt contains one valid JSON string per line.\n"
  "pathIn.txt contains one valid search path per line.\n"
  "Resulting values will be printed, tab-separated, in out.txt.\n"
  "\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

static struct slName *lineFileToList(struct lineFile *lf)
/* Return a list of lines in lf. */
{
struct slName *list = NULL;
char *line;
int size;
while (lineFileNext(lf, &line, &size))
    slAddHead(&list, slNameNewN(line, size));
slReverse(&list);
return list;
}

void shallowPrintEl(FILE *f, struct jsonElement *el, char *name)
/* Print abbreviated version if el is an object or list, or the value if object is a scalar. */
{
if (name != NULL)
    {
    fprintf(f, "\"%s\": ", name);
    }
switch (el->type)
    {
    case jsonObject:
        {
        struct hash *hash = jsonObjectVal(el, "el");
        struct hashEl *helList = hashElListHash(hash);
	fprintf(f, "{");
        if (helList)
            {
            shallowPrintEl(f, helList->val, helList->name);
            if (helList->next)
                fputs(",...", f);
            hashElFreeList(&helList);
            }
        fputc('}', f);
        }
        break;
    case jsonList:
        {
        struct slRef *vals = jsonListVal(el, name);
	fprintf(f, "[");
        if (vals)
            {
            shallowPrintEl(f, vals->val, NULL);
            if (vals->next)
                fputs(",...", f);
            }
        fputc(']', f);
        }
        break;
    case jsonString:
        {
	char *escaped = jsonStringEscape(el->val.jeString);
	fprintf(f, "\"%s\"",  escaped);
	freez(&escaped);
        }
	break;
    case jsonBoolean:
        {
	char *val = (el->val.jeBoolean ? "true" : "false");
	fprintf(f, "%s", val);
        }
	break;
    case jsonNumber:
	fprintf(f, "%ld", el->val.jeNumber);
	break;
    case jsonDouble:
	fprintf(f, "%g", el->val.jeDouble);
        break;
    case jsonNull:
        fprintf(f, "null");
        break;
    default:
        errAbort("shallowPrintEl: invalid type: %d", el->type);
        break;
    }
}

#define MAX_JSON 50

static void jsonQueryTest(char *jsonFile, char *pathFile, char *outFile)
/* Read in JSON and paths from files and print the results. */
{
struct lineFile *jsonLf = lineFileOpen(jsonFile, TRUE);
struct lineFile *pathLf = lineFileOpen(pathFile, TRUE);
FILE *outF = mustOpen(outFile, "w");
struct slName *paths = lineFileToList(pathLf);
char *line;
int size;
while (lineFileNext(jsonLf, &line, &size))
    {
    if (size <= MAX_JSON)
        fprintf(outF, "# JSON: '%s'\n", line);
    else
        fprintf(outF, "# JSON: '%-*s...'\n", MAX_JSON, line);
    struct jsonElement *top = jsonParse(line);
    struct slRef topRef;
    topRef.next = NULL;
    topRef.val = top;
    struct slName *path;
    for (path = paths;  path != NULL;  path = path->next)
        {
        fprintf(outF, "# path: '%s'\n", path->name);
        struct errCatch *errCatch = errCatchNew();
        if (errCatchStart(errCatch))
            {
            struct slRef *refResults = jsonQueryElementList(&topRef, "test JSON", path->name, NULL);
            if (refResults == NULL)
                fputs("NULL\n", outF);
            else
                {
                struct slRef *ref;
                for (ref = refResults;  ref != NULL;  ref = ref->next)
                    {
                    struct jsonElement *el = ref->val;
                    shallowPrintEl(outF, el, NULL);
                    if (ref->next)
                        fputc('\t', outF);
                    }
                fputc('\n', outF);
                }
            }
        errCatchEnd(errCatch);
        if (errCatch->gotError)
            fprintf(outF, "# failed: %s", errCatch->message->string);
        errCatchFree(&errCatch); 
        }
    fputc('\n', outF);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
jsonQueryTest(argv[1], argv[2], argv[3]);
return 0;
}
