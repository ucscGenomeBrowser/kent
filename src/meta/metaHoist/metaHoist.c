/* metaHoist - Move tags that are shared by all siblings up a level.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "meta.h"

boolean trimEmpty = FALSE;
boolean withParent = FALSE;
int indent = META_DEFAULT_INDENT;
char *keyTag="meta";
char *heavy = NULL;
char *parentTag = "parent";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "metaHoist - Move tags that are shared by all siblings up a level.\n"
  "usage:\n"
  "   metaHoist inFile outFile\n"
  "options:\n"
  "   -trimEmpty - If set then trim stanzas that are empty after lifting.\n"
  "   -withParent - If set include parent in output.\n"
  "   -heavy=tag - Define a tag not hoisted even if all siblings agree\n"
  "   -keyTag=name - Defines key tag that starts stanzas\n"
  "   -parentTag=name - Defines tag used as parent\n"
  "   -indent=N - Define how much to indent each level, default %d\n"
  , indent
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"trimEmpty", OPTION_BOOLEAN},
   {"withParent", OPTION_BOOLEAN},
   {"heavy", OPTION_STRING},
   {"keyTag", OPTION_STRING},
   {"parentTag", OPTION_STRING},
   {"indent", OPTION_INT},
   {NULL, 0},
};


boolean metaLocalTagRemove(struct meta *meta, char *tag)
/* Find given variable in list and remove it. Returns TRUE if it
 * actually removed it,  FALSE if it never found it. */
{
struct metaTagVal **ln = &meta->tagList;
struct metaTagVal *v;
for (v = *ln; v != NULL; v = v->next)
    {
    if (sameString(v->tag, tag))
        {
	*ln = v->next;
	return TRUE;
	}
    ln = &v->next;
    }
return FALSE;
}

void hoistOne(struct meta *meta, char *tag, char *val)
/* We've already determined that tag exists and has same value in all children.
 * What we do here is add it to ourselves and remove it from children. */
{
metaAddTag(meta, tag, val);
struct meta *child;
for (child = meta->children; child != NULL; child = child->next)
    metaLocalTagRemove(child, tag);
metaSortTags(meta);
}

struct slName *tagsInAny(struct meta *metaList)
/* Return list of variables that are used in any node in list. */
{
struct hash *tagHash = hashNew(6);
struct slName *tag, *tagList = NULL;
struct meta *meta;
for (meta = metaList; meta != NULL; meta = meta->next)
    {
    struct metaTagVal *v;
    for (v = meta->tagList; v != NULL; v = v->next)
        {
	if (!hashLookup(tagHash, v->tag))
	    {
	    tag = slNameAddHead(&tagList, v->tag);
	    hashAdd(tagHash, tag->name, tag);
	    }
	}
    }
hashFree(&tagHash);
return tagList;
}

char *allSameVal(char *tag, struct meta *metaList)
/* Return value of tag if it exists and is the same in each meta on list */
{
char *val = NULL;
struct meta *meta;
for (meta = metaList; meta != NULL; meta = meta->next)
    {
    char *oneVal = metaLocalTagVal(meta, tag);
    if (oneVal == NULL)
        return NULL;
    if (val == NULL)
        val = oneVal;
    else
        {
	if (!sameString(oneVal, val))
	    return NULL;
	}
    }
return val;
}

void metaTreeHoist(struct meta *meta)
/* Move tags that are the same in all children up to parent. */
{
/* Do depth first recursion, but get early return if we're a leaf. */
struct meta *child;
if (meta->children == NULL)
    return;
for (child = meta->children; child != NULL; child = child->next)
    metaTreeHoist(child);

/* Build up list of tags used in any child. */
struct slName *tag, *tagList = tagsInAny(meta->children);

/* Go through list and figure out ones that are same in all children. */
for (tag = tagList; tag != NULL; tag = tag->next)
    {
    if (!sameString(tag->name, keyTag))
	{
	if (heavy == NULL || !sameString(tag->name, heavy))
	    {
	    char *val;
	    val = allSameVal(tag->name, meta->children);
	    if (val != NULL)
		hoistOne(meta, tag->name, val);
	    }
	}
    }
slFreeList(&tagList);
}



void metaHoist(char *inFile, char *outFile)
/* metaHoist - Move tags that are shared by all siblings up a level.. */
{
struct meta *metaList = metaLoadAll(inFile, keyTag, parentTag, FALSE, FALSE);
struct meta *meta;
for (meta = metaList; meta != NULL; meta = meta->next)
    metaTreeHoist(meta);
metaWriteAll(metaList, outFile, indent, withParent, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
trimEmpty = optionExists("trimEmpty");
withParent = optionExists("withParent");
indent = optionInt("indent", indent);
keyTag = optionVal("keyTag", keyTag);
heavy = optionVal("heavy", heavy);
parentTag = optionVal("parentTag", parentTag);
if (argc != 3)
    usage();
metaHoist(argv[1], argv[2]);
return 0;
}
