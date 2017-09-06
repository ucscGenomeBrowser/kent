/* tagStormReformat - reformat tag storm file */

#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "tagStorm.h"
#include "errAbort.h"

boolean withParent = FALSE;
boolean flatten = FALSE;
boolean tab = FALSE;
boolean leaves = FALSE;
char *idTag = NULL;
int maxDepth = BIGNUM;
boolean reverseStanzas = FALSE;
char *sortOn = NULL;
boolean sort = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormReformat - reformat tag storm file.\n"
  "usage:\n"
  "   tagStormReformat input output\n"
  "options:\n"
  "   -withParent  If set a parent tag will be added. You'll need the idTag option too\n"
  "   -idTag=name Name of tag to used as primary identifier\n"
  "   -maxDepth=n Maximum depth of storm to write\n"
  "   -tab   If set will be output in tab-separated format\n"
  "   -flatten If set output all fields in each record\n"
  "   -leaves If set will only output nodes with no children\n"
  "   -reverseStanzas - Reverse order of stanzas\n"
  "   -sort Sort tags within stanzas\n"
  "   -sortOn=fieldA,fieldB,fieldC - sort tags within stanzas with fields A, B, & C before others\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"withParent", OPTION_BOOLEAN},
   {"idTag", OPTION_STRING},
   {"maxDepth", OPTION_INT},
   {"flatten", OPTION_BOOLEAN},
   {"tab", OPTION_BOOLEAN},
   {"leaves", OPTION_BOOLEAN},
   {"reverseStanzas", OPTION_BOOLEAN},
   {"sort", OPTION_BOOLEAN},
   {"sortOn", OPTION_STRING},
   {NULL, 0},
};

void tagStormReverseStanzas(struct tagStorm *tagStorm, struct tagStanza **pList)
/* Recursively reverse all stanzas */
{
slReverse(pList);
struct tagStanza *stanza;
for (stanza = *pList; stanza != NULL; stanza = stanza->next)
    if (stanza->children != NULL)
        tagStormReverseStanzas(tagStorm, &stanza->children);
}

void tagStormReformat(char *input, char *output)
/* Write input to output with possibly some conversions */
{
struct tagStorm *tagStorm = tagStormFromFile(input);
if (sort)
    tagStormAlphaSort(tagStorm);
if (sortOn != NULL)
    {
    int fieldCount = chopByChar(sortOn, ',', NULL, 0);
    char *fields[fieldCount];
    chopByChar(sortOn, ',', fields, fieldCount);
    tagStormOrderSort(tagStorm, fields, fieldCount);
    }
if (reverseStanzas)
    tagStormReverseStanzas(tagStorm, &tagStorm->forest);
if (tab)
    tagStormWriteAsFlatTab(tagStorm, output, idTag, withParent, maxDepth, leaves, "n/a", TRUE);
else if (flatten)
    tagStormWriteAsFlatRa(tagStorm, output, idTag, withParent, maxDepth, leaves);
else
    tagStormWrite(tagStorm, output, maxDepth);
tagStormFree(&tagStorm);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
idTag = optionVal("idTag", idTag);
withParent = optionExists("withParent");
flatten = optionExists("flatten");
tab = optionExists("tab");
leaves = optionExists("leaves");
sort = optionExists("sort");
sortOn = optionVal("sortOn", sortOn);
reverseStanzas = optionExists("reverseStanzas");
if (withParent && !idTag)
    errAbort("Please specify idTag option if using withParent option");
maxDepth = optionInt("maxDepth", maxDepth);
tagStormReformat(argv[1], argv[2]);
return 0;
}
