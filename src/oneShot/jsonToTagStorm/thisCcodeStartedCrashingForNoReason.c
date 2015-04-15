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


void beUgly(char *line)
/* The code was getting so ugly I decided to embrace it. This function is ugly. 
 * The function uses four for loops, each embedded into the other to split apart a
 * .json single line file into name/value pairs for a tagstorm file. */
{
printf("sanity check");
char *lineCopy = cloneString(line);
int arraySize = countSeparatedItems(line, *"[");
char *chopByArray[arraySize]; 
chopString(lineCopy, "[", chopByArray, arraySize);
int i;
printf("Before Loops");
for (i = 0 ; i <= arraySize-1; ++i)
    {
    char *arrayLine = cloneString(chopByArray[i]);
    int childrenSize = countSeparatedItems(arrayLine, *"{");
    char *chopByChildren[childrenSize];
    chopString (arrayLine, "{", chopByChildren, childrenSize);
    int j;
    printf("End loop 1");
    for (j = 0 ; j <= childrenSize-1; ++j)
        {
	char *childrenLine = cloneString(chopByChildren[j]);
	int commaSize = countSeparatedItems(childrenLine, *",");
	char *chopByComma[commaSize];
	chopString (childrenLine, ",", chopByComma, commaSize);
	int k;
	printf("End loop 2");
	for (k = 0 ; k <= commaSize-1; ++k)
	    {
	    char *commaLine = cloneString(chopByComma[k]);
	    int colonSize = countSeparatedItems(commaLine, *":");
	    char *chopByColon[colonSize];
	    chopString (commaLine, ":", chopByColon, colonSize);
	    removePunctuation(chopByColon[0]);
	    removePunctuation(chopByColon[1]);
    	    printf("End loop 3");
	    }
	}
    }
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
beUgly(lineCopy);
exit(0);
int arraySize = countSeparatedItems(line, *"[");
char *chopByArray[arraySize]; 
chopString(lineCopy, "[", chopByArray, arraySize);
int i, index = 0;
char *stanzas [10000]; 
stanzas[0]="temp";
for (i = 0 ; i <= arraySize-1; ++i)
    {
    printf("%s\n",chopByArray[i]);
    char *newLine = cloneString(chopByArray[i]);
    int childrenSize = countSeparatedItems(newLine, *"{");
    char *chopByChildren[childrenSize];
    chopString (newLine, "{", chopByChildren, childrenSize);
    int j;
    for (j = 0 ; j <= childrenSize-1; ++j)
        {
	if (i == 0 &&j == 1) continue;
	if (lastNonwhitespaceChar(chopByChildren[j]) && chopByChildren[j] != NULL) 
	    {
	    stanzas[index] = cloneString(chopByChildren[j]);
	    ++ index;
	    }
	}
    }
// stanzas holds all the stanza's for the tagstorm file. Each stanza is stored as a string, there are index many stanzas. 

int depth = -2; // I wish I had a good reason for this, but I dont. Forces stanza's into the right orrientation
int correctBrother = 0;  // For adding children to a child that is not firstborn 
int j;
for (j = 0 ; j <= index; ++j)
    /* */
    {    
    int commaSize = countSeparatedItems(stanzas[j], *",");
    int stanzaDepth = countSeparatedItems(stanzas[j], *"]");
    int prevDepth = depth;
    char *chopByComma[commaSize]; 
    // Each string in chopByComma contains a name value pair in the form
    // "'name': 'value'". 
    chopString(cloneString(stanzas[j]), ",", chopByComma, commaSize);
    int k;
    struct slPair *tagNameValue = NULL;
    for (k = 0; k <= commaSize ; ++k)
	{
	if (chopByComma[k]==NULL) continue; 
	printf("%s\n",chopByComma[k]);
	char *chopByColon[2];
	printf("%s\n",chopByComma[k]);
	chopString(cloneString(chopByComma[k]), ":", chopByColon, 2);
	removePunctuation(chopByColon[0]);
	removePunctuation(chopByColon[1]);
	if (lastNonwhitespaceChar(chopByColon[1]) == NULL)
	    /* Children will have a null value, they also mark a decrease in depth*/
	    {
	    ++depth;
	    continue;
	    }
	printf("A final name value pair is %s , %s \n", chopByColon[0], chopByColon[1]); 
    	slPairAdd(&tagNameValue, chopByColon[0], chopByColon[1]);
	/* This breaks the samples apart into name value pairs, with a bunch of uneccessary 
	 * punctuation. Also the 'children' name may not have a value */
	}
    continue;
    /* Create a new tagStorm stanza here, also be ready for some real
     * dificulties with stanza depth, try using the stanzaDepth int up above -Chris */ 
    struct tagStanza *stanza= NULL;
    printf("this is the depth, %i \n", depth);
    if (depth <= 0)
        {
        stanza = tagStanzaNewAtEnd(tags, NULL);
	printf("is this proccing?\n");
	}
    else 
        {
	int i;
	struct tagStanza *rightParent = tags->forest;
	for (i=0; i<depth-1; ++i)
	/* remarkably this seems to be working how it was designed, 
	 * however it does not fully address the issue, as written it only seems 
	 * to handle finding the correct depth level, (IE correct parent), but not
	 * the correct location in the level (IE correct brother/sister). Hopefully
	 * this can be worked around by using two counters for depth, and moving to the
	 * next brother/sister when the depth decreases*/ 
	    {
	    rightParent = rightParent->children;
	    if (depth < prevDepth)
	        {
		++correctBrother;
		printf("is this proccing?\n");
		}
	    if (correctBrother > 0)
	        {
		printf("this is the ugliest code I have ever forced this far into production\n");
		int k;
		for (k=0; k<correctBrother; ++k)
		    {
		    rightParent = rightParent->next;
		    }
		}
	    }
    	stanza = tagStanzaNewAtEnd(tags, rightParent);
        }
    struct slPair *superTemp = NULL;
    for (superTemp = tagNameValue ; superTemp != NULL ; superTemp = superTemp->next)
        {
        tagStanzaAdd(tags, stanza, superTemp->name, superTemp->val); 
        }
    -- stanzaDepth;
    depth -= stanzaDepth;
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
