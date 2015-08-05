/* jsonToTagStorm - Converts json format files into tagStorm format files.  Json array names are
 * not represented in tagStorm format and will be lost. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jsonWrite.h"
#include "tagStorm.h"
#include "tokenizer.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "jsonToTagStorm - Convert a .json file into a .tagStorm file. The .json file"
  "  must adhere to .json convention (please see json.org), multiple lined .json files are allowed. \n"
  "usage:\n"
  "   jsonToTagStorm in.json out.tags\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void parseJson(struct lineFile *lf, FILE *f)
/* This function uses a tokenizer to tokenize a .json file, 
 * the tokens are iterated over once. Depth is updated as the depth character
 * '[' is encountered.  Name value pairs are deciphered and printed to 
 * the correct depth. */ 
{
int i;
int depth = 0;
struct tokenizer *tkz = tokenizerOnLineFile(lf);
char *curToken = tokenizerNext(tkz); 
char *name = NULL, *val = NULL; 
bool beforeVal = TRUE, expVal = FALSE, firstLine = TRUE; 
int count = 0, preventDoubleLineBreak = 0; 
for (; curToken != NULL; curToken = tokenizerNext(tkz))
    // Iterate over all the tokens in the input file 
    {
    ++count; 
    if (!strcmp(curToken, "["))
    	// Update depth and handle .json arrays 
        {
	if (expVal)
	// notices a new array
	    {
	    if (strcmp(name,"children")) warn("WARNING: A .json array was encountered with a name other than 'children', the array name '%s' will be lost upon conversion.", name); 
	    expVal = FALSE;
	    beforeVal = TRUE; 
	    }
	if (firstLine)
	    // Dont update depth for first '[' and dont print a newline
	    {
	    firstLine = FALSE;
	    continue;
	    }
	++depth; 
	fprintf(f,"\n"); 
	continue; 
	}
    if (!strcmp(curToken, "]"))
    	// Update depth
        {
	--depth; 
	continue; 
	}
    if (!strcmp(curToken, "}"))
        {
	if (preventDoubleLineBreak == count - 2)
	    {
	    continue; 
	    }
	preventDoubleLineBreak = count;
	fprintf(f,"\n"); 
	continue; 
	}
    if (!strcmp(curToken, "{"))
        {
	if (expVal & !beforeVal) errAbort("ERROR: The .json object named '%s' can not be converted to tagStorm format. Please remove this object, or reformat it, then try again. Aborting.", name);
        continue;
	}
    if (!strcmp(curToken, ":"))
    	// Get ready for a value
        {
	expVal = TRUE;
	continue;
	} 
    if (!strcmp(curToken, ","))
    	// Reset name and value
        {
	name=NULL; 
	val=NULL;
	beforeVal = TRUE; 
	continue; 
	}
    if (beforeVal) 
    	// Get the name
        {
	name = cloneString(curToken);
	beforeVal = FALSE; 
	}
    else
        // Get the value, print name and value to propper depth
        {
	// consider using spaceOut(f, depth*4);
	for (i = 0; i < depth; ++i)
	    {
	    fprintf(f,"    "); 
	    }
	if (skipToSpaces(name)!=NULL) errAbort("ERROR: There seems to be whitespace in a tag's name field, the name is '%s' . Please provide a different name with no white space, aborting.", name);
	fprintf(f,"%s ", replaceChars(name, "\\\"", "\"")); 
	val = curToken; 
	fprintf(f,"%s\n", replaceChars(val, "\\\"", "\"")); 
	} 
    }
}


void jsonToTagStorm(char *input, char *output)
/* jsonToTagStorm - Converts .json files into .tagStorm files */
{
char cmd[1024];
// perl command to remove newlines and do a bit of formatting 
safef(cmd, 1024, "perl -wpe 's/\\n//' %s > true%s", input, input);
mustSystem(cmd);
char cleanup[512], trueFile[512];
safef(cleanup, 512, "rm true%s", input);
safef(trueFile, 512, "true%s", input);
struct lineFile *lf = lineFileOpen(trueFile, TRUE);
FILE *f = mustOpen(output, "w");
mustSystem(cleanup);
parseJson(lf, f);
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
