/* jsonToTagStorm - Some example json output stuff. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jsonWrite.h"
#include "tagStorm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "jsonToTagStorm - Convert a .json file into a .tagStorm file. The .json file"
  " must be in a very specific format...\n"
  "usage:\n"
  "   jsonToTagStorm in.json out.tags\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void rWriteJson(struct jsonWrite *jw, char *label, struct tagStanza *stanzaList)
{
jsonWriteListStart(jw, label);
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    jsonWriteObjectStart(jw, NULL);
    struct slPair *pair;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        jsonWriteString(jw, pair->name, pair->val);
    if (stanza->children != NULL)
        rWriteJson(jw, "children", stanza->children);
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
}


void readJsonStanza(char* line)
// Gather all the information in the current stanza, a .json stanza is defined
// here as any block of json code between either { and } or [ and ]
{

}

struct slPair prepLine(char *line)
/* singlye line json is ugly, and the code is worse.
 * A function to help with removing punctuation*/
{
char *choppedLine[1024];
int size, i;
size = countSeparatedItems(line, *",");
chopString(line, ",",choppedLine,size);
struct slPair *pair;
AllocVar(pair);
for (i=0 ; i <= size-1 ; ++i)
    {
    char *nameValPair[2]; 
    chopString(choppedLine[i], ":", nameValPair, countSeparatedItems(choppedLine[i], *":"));
    eraseNonAlphaNum(nameValPair[0]);
    eraseNonAlphaNum(nameValPair[1]);
    if (strcmp(nameValPair[0],"Children")) continue; 
    slPairAdd(&pair, nameValPair[0], nameValPair[1]);
    }
return *pair;
}

void jsonToTagStorm(char *input, char *output)
/* jsonToTagStorm - Some example json output stuff. */
{
//struct tagStorm *tags = tagStormNew(output);
struct lineFile *lf = lineFileOpen(input, TRUE);
char *line;
if (!lineFileNext(lf, &line, NULL))
    errAbort("There doesn't seem to be any text in this .json file, %s", input);
printf(" %s ", line);
char *lineCopy = cloneString(line);
char *choppedLine[1024]; 
int size = countSeparatedItems(line, *"[");
printf("The size is %i \n", size);
chopString(lineCopy, "[", choppedLine, size);
int i, index = 0;
char *aBetterChoppedLine[1000]; 
for (i = 0 ; i <= size-1; ++i)
    {
    char *newLine = cloneString(choppedLine[i]);
    int newSize = countSeparatedItems(newLine, *"{");
    printf(" The first chop made this line %s  the size of the next for loop is %i \n", newLine, newSize);
    char *newChoppedLine[1024];
    chopString (newLine, "{", newChoppedLine, newSize);
    int j;
    for (j = 0 ; j <= newSize-1; ++j)
        {
	printf(" The second chop made this line %s \n", newChoppedLine[j]);
	aBetterChoppedLine[index] = cloneString(newChoppedLine[j]);
	++ index;
	}
    }
int j = ArraySize(aBetterChoppedLine);
for (j = 0 ; j <= ArraySize(aBetterChoppedLine); ++j)
    {
    if (aBetterChoppedLine[j] == NULL) continue;
    printf("The current line woulb be %s \n", aBetterChoppedLine[j]);
    }

printf(" What we got going on here..? %i \n", chopString(line, "{", NULL, size));

FILE *f = mustOpen(output, "w");
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
jsonToTagStorm(argv[1], argv[2]);
return 0;
}
