/* jsonToTagStorm - Converts json format files into tagStorm format files.  Json array names are
 * not represented in tagStorm format and will be lost. */
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
  " must adhere to .json convention, multiple lines .json files are allowed. \n"
  "usage:\n"
  "   jsonToTagStorm in.json out.tags\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


void removePunctuation(char *s)
/* Remove the remaining unwanted punctuation in a string */
{
stripString(s,"\"");
stripString(s,"]");
stripString(s,"}");
}


void prettyPrint(char *name, char *value, int depth, FILE *f)
/* Print the name value tag to the corresponding depth */
{
int i;
for (i = 0; i < depth; ++i)
    {
    fprintf(f,"    "); 
    }
fprintf(f,"%s %s\n", name, value); 
}


void parseJson(char *line, FILE *f)
 /* This function uses three embedded 'for' loops to split apart a
 * .json single line file into name/value pairs for a tagstorm file. */
{
int arraySize = chopString(line, "[", NULL, 0);
// First break the string apart by arrays
char *chopByArray[arraySize]; 
chopString(line, "[", chopByArray, arraySize);
int i;
int depth = 0;
for (i = 1 ; i < arraySize; ++i)
    {
    char *arrayLine = chopByArray[i];
    int childrenSize = chopString(arrayLine, "{", NULL, 0);
    // Break the arrays apart into objects
    char *chopByChildren[childrenSize];
    chopString (arrayLine, "{", chopByChildren, childrenSize);
    int j;
    for (j = 0 ; j < childrenSize; ++j)
	// Iterate over the objects, at this point each object 
	// contains the information required for a single tagStorm stanza. 
        {
	char *childrenLine = chopByChildren[j];
	int commaSize = countSeparatedItems(childrenLine, ',');
	// Break the list of name values apart 
	char *chopByComma[commaSize];
	int updateDepth = countSeparatedItems(childrenLine, ']') - 1;
	chopString (childrenLine, ",", chopByComma, commaSize);
	int k;
	bool newArray = FALSE; 
	for (k = 0 ; k < commaSize; ++k)
	    //Iterate over all the name value pairs in a given stanza 
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
	        //Ignore array names, they are not represented in tagStorm format. However 
		//opening an array signifies an increase in depth. 
	        {
		++depth; 
		newArray = TRUE; 
		continue;
		}
	    prettyPrint(trimSpaces(chopByColon[0]), trimSpaces(chopByColon[1]), depth, f);
	    }
	depth -= updateDepth;
	// Update the depth after each stanza is printed
	if (!newArray) fprintf(f,"\n");
	// Print a newline after each stanza
	}
    }
}


void jsonToTagStorm(char *input, char *output)
/* jsonToTagStorm - Converts .json files into .tagStorm files */
{
char cmd[1024];
safef(cmd, 1024, "perl -wpe 's/\\n//' %s > true%s", input, input);
mustSystem(cmd);
char cleanup[512], trueFile[512];
safef(cleanup, 512, "rm true%s", input);
safef(trueFile, 512, "true%s", input);
struct lineFile *lf = lineFileOpen(trueFile, TRUE);
char *line;
if (!lineFileNext(lf, &line, NULL))
    errAbort("There doesn't seem to be any text in this .json file, %s", input);
FILE *f = mustOpen(output, "w");
mustSystem(cleanup);
parseJson(line, f);
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
