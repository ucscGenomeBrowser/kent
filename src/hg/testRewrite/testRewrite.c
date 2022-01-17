/* testRewrite - Test harness for cart rewrite tool. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cartRewrite.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testRewrite - Test harness for cart rewrite tool\n"
  "usage:\n"
  "   testRewrite input output\n"
  "options:\n"
  "   -script=sed-like script\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"script", OPTION_STRING},
   {NULL, 0},
};

char *script;

#ifdef NOTNOW
struct snippet
{
struct snippet *next;
int num;
char *precursor;
int precursorLen;
};

struct edit
{
struct edit *next;
regex_t *compiledExp;
struct snippet *snippets;
};

struct snippet *parseSnippets(char *input)
{
char *in = input;
struct snippet *out = NULL;

char *prev = input;
for(;;)
    {
    if ((*in == 0) || ((*in == '\\') && isdigit(in[1])))
        {
        struct snippet *snippet;
        AllocVar(snippet);

        int size = in - prev;
        if (size)
            {
            char buffer[size + 1];
            strncpy(buffer, prev, size);
            buffer[size] = 0;
            snippet->precursor = cloneString(buffer);
            snippet->precursorLen = size;
            prev = in;
            }

        if (*in)
            {
            in++;
            snippet->num = atoi(in);
            while (isdigit(*in))
                in++;
            }

        slAddHead(&out, snippet);
        if (*in == 0)
            break;
        }
    else
        in++;
    }

slReverse(&out);

return out;
}

struct edit *parseEdit(char *line, char *file, int lineNum)
{
char *strings[3];

int count =  chopString(line, "\t", strings, 3);

if (count != 2)
    errAbort("Line %d of %s doesn't have exactly two tab separated fields.\n", lineNum, file);

regex_t *compiledExp = NULL;
int errNum = 0;
int compileFlags = 0;

AllocVar(compiledExp);
errNum = regcomp(compiledExp, strings[0], compileFlags);

if (errNum != 0)
    {
    char errBuf[4096];
    regerror(errNum, compiledExp, errBuf, sizeof(errBuf));
    errAbort("got regular expression compilation error %d: %s on line %d of %s", errNum, errBuf, lineNum, file);
    }

struct edit *edit;
AllocVar(edit);
edit->compiledExp = compiledExp;
edit->snippets = parseSnippets(strings[1]);

return edit;
}

char *doSubEdits(struct snippet *snippets, regmatch_t *matches, char *source, int *plength)
{
char output[40 * 1024], *out = output;
*out = 0;
//for(; matches->rm_so != -1;matches++, snippets = snippets->next) 
for(; snippets ; matches++, snippets = snippets->next)
    {
    // copy the part before the match
    strncpy(out, snippets->precursor, snippets->precursorLen);
    out += snippets->precursorLen;
    *plength += snippets->precursorLen;

    if (matches->rm_so == -1)
        break;

    // copy in the part that matches the regular expression
    int size = matches->rm_eo - matches->rm_so;
    strncpy(out, &source[matches->rm_so], size);
    *plength += size;
    out += size;
    }
*out = 0;
return cloneString(output);
}

char *doOneEdit(struct edit *edit, char *inputString)
{
char buffer[40 * 1024];
char *source = inputString;
regmatch_t matches[1024];
int lastSrc = 0;
int offset = 0;
int inputLen = strlen(inputString);

for(;;)
    {
    if (regexec(edit->compiledExp, source, ArraySize(matches), matches, 0))
        break;

    int size =  matches->rm_so;
    strncpy(&buffer[lastSrc], source, size);
    lastSrc += size;

    int subSize = 0;
    char *subEdit = doSubEdits(edit->snippets, matches+1, source, &subSize);
    strncpy(&buffer[lastSrc], subEdit, subSize);

    lastSrc += subSize;
    offset += matches->rm_eo;
    if (offset >= inputLen)
        printf("foo");
    source = &inputString[offset];
    }

strcpy(&buffer[lastSrc], source);

return cloneString(buffer);
}

void doEdits(struct edit *edits, char *input, char *outputFile)
{
FILE *f = mustOpen(outputFile, "w");

char *outString = input;

for(; edits; edits = edits->next)
    outString = doOneEdit(edits, outString);
fputs(outString, f);
fclose(f);
}
#endif

void testRewrite(char *inputFile, char *outputFile)
/* testRewrite - Test harness for cart rewrite tool. */
{
/*
if (script)
    {
    struct lineFile *lf = lineFileOpen(script, TRUE);
    char *start;
    int size;
    struct edit *edits = NULL;

    while (lineFileNext(lf, &start, &size))
        {
        slAddHead(&edits, parseEdit(start, script, lf->lineIx));
        }
    slReverse(&edits);
    lineFileClose(&lf);
    }
else
*/


int count = 0;
    char *start;
    int size;
FILE *f = mustOpen(outputFile, "w");
struct lineFile * lf = lineFileOpen(inputFile, TRUE);
while (lineFileNext(lf, &start, &size))
    {
    if (count)
        errAbort("input should have only one line with name=value pairs separated by ampersands.");

//    doEdits(edits, start, outputFile);
    char *output = regexEdit(cartRewrites[0].editArray,cartRewrites[0].numEdits , start, FALSE);
    fputs(output, f);
    
    count++;
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
script = optionVal("script", NULL);
testRewrite(argv[1], argv[2]);
return 0;
}
