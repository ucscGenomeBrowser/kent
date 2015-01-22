/* metaFlatten - Flatten meta.ra file so all stanzas include all tags.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "meta.h"

boolean withParent = FALSE;
boolean indent = META_DEFAULT_INDENT;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "metaFlatten - Flatten meta.ra file so all stanzas include all tags.\n"
  "usage:\n"
  "   metaFlatten input output\n"
  "options:\n"
  "   -withParent - if set write out parent tags in each stanze\n"
  "   -indent - amount to indent per level of nesting.  If 0, you better set -withParent too.\n"
  "             Default is %d\n"
  , indent
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"withParent", OPTION_BOOLEAN},
   {"indent", OPTION_INT},
   {NULL, 0},
};

static void addNewTags(struct meta *meta, struct metaTagVal *mtvList)
/* Add all tags in list that are not already in meta to meta. */
{
struct metaTagVal *mtv;
for (mtv = mtvList; mtv != NULL; mtv = mtv->next)
    {
    if (metaLocalTagVal(meta, mtv->tag) == NULL)
        {
	struct metaTagVal *newMtv;
	AllocVar(newMtv);
	newMtv->tag = mtv->tag;
	newMtv->val = mtv->val;
	slAddTail(&meta->tagList, newMtv);
	}
    }
}

static void rMetaFlatten(struct meta *metaList)
/* Add tags from parents to our own tags (so long as they don't conflict */
{
struct meta *meta;
for (meta = metaList; meta != NULL; meta = meta->next)
    {
    if (meta->parent)
        addNewTags(meta, meta->parent->tagList);
    metaSortTags(meta);
    if (meta->children)
        rMetaFlatten(meta->children);
    }
}

void metaFlatten(char *input, char *output)
/* metaFlatten - Flatten meta.ra file so all stanzas include all tags.. */
{
struct meta *metaList = metaLoadAll(input, "meta", "parent", FALSE, FALSE);
rMetaFlatten(metaList);
metaWriteAll(metaList, output, indent, withParent, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
indent = optionInt("indent", indent);
withParent = optionExists("withParent");
metaFlatten(argv[1], argv[2]);
return 0;
}
