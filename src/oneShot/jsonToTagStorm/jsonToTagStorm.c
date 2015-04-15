/* jsonWriteTest - Some example json output stuff. */
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
  "jsonToTagStorm - Convert a .tagStorm file into a .json file. No information is lost. \n"
  "usage:\n"
  "   jsonWriteTest in.tags out.json\n"
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
char *lineCopy = cloneString(line);
int arraySize = countSeparatedItems(line, *"[");
char *chopByArray[arraySize]; 
chopString(lineCopy, "[", chopByArray, arraySize);
int i;
//printf("Before Loops %s\n", line);
for (i = 0 ; i <= arraySize-1; ++i)
    {
    char *arrayLine = cloneString(chopByArray[i]);
    int childrenSize = countSeparatedItems(arrayLine, *"{");
    printf("End loop 1 %s, %i\n", arrayLine, childrenSize);
    char *chopByChildren[childrenSize];
    chopString (arrayLine, "{", chopByChildren, childrenSize);
    int j;
    for (j = 0 ; j <= childrenSize-1; ++j)
        {
	char *childrenLine = cloneString(chopByChildren[j]);
	int commaSize = countSeparatedItems(childrenLine, *",");
	char *chopByComma[commaSize];
	//printf("End loop 2, %s, %i , %i\n", childrenLine, commaSize, j);
	chopString (childrenLine, ",", chopByComma, commaSize);
	int k;
	for (k = 0 ; k <= commaSize-1; ++k)
	    {
	    char *commaLine = cloneString(chopByComma[k]);
	    //printf("End loop 3, %s\n", commaLine);
	    int colonSize = countSeparatedItems(commaLine, *":");
	    char *chopByColon[colonSize];
	    chopString (commaLine, ":", chopByColon, colonSize);
	    }
	}
    }
}
void testFunction(char *line)
{
}

void jsonWriteTest(char *input, char *output)
/* jsonWriteTest - Some example json output stuff. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
char *line;
if (!lineFileNext(lf, &line, NULL))
    errAbort("There doesn't seem to be any text in this .json file, %s", input);
char *lineCopy = cloneString(line);
testFunction("Hello World");
beUgly(lineCopy);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
jsonWriteTest(argv[1], argv[2]);
return 0;
}
