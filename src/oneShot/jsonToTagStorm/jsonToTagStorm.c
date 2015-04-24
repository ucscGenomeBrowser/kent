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


void removePunctuation(char *s)
/* Remove all punctuation in a string */
{
stripString(s,"\"");
stripString(s,"]");
stripString(s,"}");
stripString(s,"[");
stripString(s,"{");
stripString(s,",");
stripString(s," ");
}


void jsonToTagStorm(char *input, char *output)
/* jsonToTagStorm - Some example json output stuff. */
{
struct tagStorm *tags = tagStormNew(output);
struct lineFile *lf = lineFileOpen(input, TRUE);
char *line;
if (!lineFileNext(lf, &line, NULL))
    errAbort("There doesn't seem to be any text in this .json file, %s", input);
char *lineCopy = cloneString(line);
char *choppedLine[1024]; 
int size = countSeparatedItems(line, *"[");
chopString(lineCopy, "[", choppedLine, size);
int i, index = 0;
char *stanzas[1000]; 
for (i = 0 ; i <= size-1; ++i)
    {
    char *newLine = cloneString(choppedLine[i]);
    int newSize = countSeparatedItems(newLine, *"{");
    char *newChoppedLine[1024];
    chopString (newLine, "{", newChoppedLine, newSize);
    int j;
    for (j = 0 ; j <= newSize-1; ++j)
        {
	stanzas[index] = cloneString(newChoppedLine[j]);
	++ index;
	}
    }
int j;
for (j = 0 ; j <= ArraySize(stanzas); ++j)
    /* Each string that is processed in this loop corresponds to a complete
     * tagStanza, this is helpful for keeping stanza's separate, but introduces
     * an issue when going to a child stanza. This issue has not been addressed
     * yet. This code seems to be working -Chris */
    {
    if (lastNonwhitespaceChar(stanzas[j]) == NULL) continue;
    // Each string in nextChop contains a name value pair in the form
    // "name": "value". 
    int depth = 0;
    int nextSize = countSeparatedItems(stanzas[j], *",");
    int stanzaDepth = countSeparatedItems(stanzas[j], *"]");
    char *nextChop[nextSize]; 
    //printf("The current chomped line is  %s endSize is %d \n", stanzas[j], stanzaDepth);
    chopString(cloneString(stanzas[j]), ",", nextChop, nextSize);
    /* Ideally break by comma's, creating a list of name value pairs.
     * This code is not working*/
    int k;
    //continue;
    struct slPair *tagNameValue = NULL;
    for (k = 0; k <= nextSize ; ++k)
    	{
	if (lastNonwhitespaceChar(nextChop[k]) == NULL) continue;
	//printf("This is the thing I am looking at %s \n", nextChop[k]);
	//continue;
	int lastSize = countSeparatedItems(nextChop[k], *":"); 
	char *finalChop[lastSize];
	chopString(cloneString(nextChop[k]), ":", finalChop, lastSize);
	removePunctuation(finalChop[0]);
	if (lastNonwhitespaceChar(finalChop[1]) == NULL)
	    {
	    ++depth;
	    continue;
	    }
	removePunctuation(finalChop[1]);
    	slPairAdd(&tagNameValue, finalChop[0], finalChop[1]);
	//printf("A final name value pair is %s , %s \n", finalChop[0], finalChop[1]); 
	/* This breaks the samples apart into name value pairs, with a bunch of uneccessary 
	 * punctuation. Also the 'children' name may not have a value */
	}
    /* Create a new tagStorm stanza here, also be ready for some real
     * dificulties with stanza depth, try using the stanzaDepth int up above -Chris */ 
    struct tagStanza *stanza= NULL;
    if (depth == 0)
        {
        stanza = tagStanzaNewAtEnd(tags, NULL);
	}
    else 
        {
    	stanza = tagStanzaNewAtEnd(tags, tags->forest);
        }
    struct slPair *superTemp = NULL;
    for (superTemp = tagNameValue ; superTemp != NULL ; superTemp = superTemp->next)
        {
        tagStanzaAdd(tags, stanza, superTemp->name, superTemp->val); 
        }
    -- stanzaDepth;
    }

FILE *f = mustOpen(output, "w");
tagStormWrite(tags, output, 100);
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
