/* tagStormToJson - Takes in a tagStorm file and converts it to a .json file.
 * Note that for name:value in a tagStorm file, there can be no whitepaces in name*/
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
  "tagStormToJson - Convert a .tagStorm file into a .json file. No information is lost, quotes in  \n"
  "name or value fields will be escaped in the .json output. \n"
  "usage:\n"
  "   jsonWriteTest in.tags out.json\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

bool first = TRUE; 

void rWriteJson(struct jsonWrite *jw, char *label, struct tagStanza *stanzaList)
/* A recursive function which iterates through a tagStorm and converts each
 * stanza to a .json object. StanzaList is a linked list that contains all the
 * root/basic nodes in the tagStorm file. */ 
{
if (first)
    // Don't print "children" before the first list
    {
    jsonWriteListStart(jw, NULL);
    first=FALSE;
    }
else{
    jsonWriteListStart(jw, label); 
    }
struct tagStanza *stanza;
// iterate over all the objects in the tagStorm
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    jsonWriteObjectStart(jw, NULL);
    struct slPair *pair;
    // Print out the tags associated with the current object
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        jsonWriteString(jw, replaceChars(pair->name, "\"", "\\\""), pair->val);
    // Check for children, if they exist start the process again. 
    if (stanza->children != NULL)
        rWriteJson(jw, "children", stanza->children);
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
}

void jsonWriteTest(char *input, char *output)
/* jsonWriteTest - Some example json output stuff. */
{
struct tagStorm *tags = tagStormFromFile(input); //Parse input file into a tagStorm struct
struct jsonWrite *jw = jsonWriteNew();	// The start of the .json output
//jsonWriteObjectStart(jw, NULL);              // Anonymous outer object
rWriteJson(jw, "children", tags->forest); //Call a recursive function starting with the first child
//jsonWriteObjectEnd(jw);			// Finish the .json object then print it. 
FILE *f = mustOpen(output, "w");	
fprintf(f, "%s\n", jw->dy->string);
carefulClose(&f);
jsonWriteFree(&jw);
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
