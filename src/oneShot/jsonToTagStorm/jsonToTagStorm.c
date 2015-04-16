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


void prettyPrint(char *name, char *value, int depth, FILE *f)
{
int i;
for (i = 0; i < depth; ++i)
    {
    fprintf(f,"    "); 
    }
fprintf(f,"%s %s\n", name, value); 
}


void beLessUgly(char *line, FILE *f)
/* The code was getting so ugly I decided to embrace it. This function is ugly. 
 * The function uses four for loops, each embedded into the other to split apart a
 * .json single line file into name/value pairs for a tagstorm file. */
{
int arraySize = chopString(line, "[", NULL, 0);
char *chopByArray[arraySize]; 
chopString(line, "[", chopByArray, arraySize);
int i;
int depth = 0;
for (i = 0 ; i < arraySize; ++i)
    {
    char *arrayLine = chopByArray[i];
    int childrenSize = chopString(arrayLine, "{", NULL, 0);
    char *chopByChildren[childrenSize];
    chopString (arrayLine, "{", chopByChildren, childrenSize);
    int j;
    for (j = 1 ; j < childrenSize; ++j)
        {
	char *childrenLine = chopByChildren[j];
	int commaSize = countSeparatedItems(childrenLine, ',');
	char *chopByComma[commaSize];
	int updateDepth = countSeparatedItems(childrenLine, ']') - 1;
	chopString (childrenLine, ",", chopByComma, commaSize);
	int k;
	for (k = 0 ; k < commaSize; ++k)
	    {
	    char *commaLine = chopByComma[k];
	    int colonSize = countSeparatedItems(commaLine, ':');
	    char *chopByColon[colonSize];
	    chopString (commaLine, ":", chopByColon, colonSize);
	    assert(colonSize > 0);
	    if (colonSize ==1) continue;
	    removePunctuation(chopByColon[0]);
	    removePunctuation(chopByColon[1]);
	    if (lastNonwhitespaceChar(chopByColon[1])==NULL)
	        {
		++depth; 
		continue;
		}
	    prettyPrint(chopByColon[0], chopByColon[1], depth, f);
	    }
	depth -= updateDepth; 
	fprintf(f,"\n");
	}
    }
}




void jsonToTagStorm(char *input, char *output)
/* jsonToTagStorm - Some example json output stuff. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
char *line;
if (!lineFileNext(lf, &line, NULL))
    errAbort("There doesn't seem to be any text in this .json file, %s", input);
FILE *f = mustOpen(output, "w");
beLessUgly(line, f);
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
