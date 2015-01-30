/* tagStormHoist - Move tags that are shared by all siblings up a level. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormHoist - Move tags that are shared by all siblings up a level\n"
  "usage:\n"
  "   tagStormHoist input output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


boolean localTagRemove(struct tagStanza *stanza, char *tag)
/* Find given variable in list and remove it. Returns TRUE if it
 * actually removed it,  FALSE if it never found it. */
{
struct slPair **ln = &stanza->tagList;
struct slPair *v;
for (v = *ln; v != NULL; v = v->next)
    {
    if (sameString(v->name, tag))
        {
	*ln = v->next;
	return TRUE;
	}
    ln = &v->next;
    }
return FALSE;
}

void hoistOne(struct tagStorm *tagStorm, struct tagStanza *stanza, char *tag, char *val)
/* We've already determined that tag exists and has same value in all children.
 * What we do here is add it to ourselves and remove it from children. */
{
tagStormUpdateTag(tagStorm, stanza, tag, val);
struct tagStanza *child;
for (child = stanza->children; child != NULL; child = child->next)
    localTagRemove(child, tag);
}

struct slName *tagsInAny(struct tagStanza *stanzaList)
/* Return list of variables that are used in any node in list. */
{
struct hash *tagHash = hashNew(6);
struct slName *tag, *tagList = NULL;
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *v;
    for (v = stanza->tagList; v != NULL; v = v->next)
        {
	if (!hashLookup(tagHash, v->name))
	    {
	    tag = slNameAddHead(&tagList, v->name);
	    hashAdd(tagHash, tag->name, tag);
	    }
	}
    }
hashFree(&tagHash);
return tagList;
}

char *allSameVal(char *tag, struct tagStanza *stanzaList)
/* Return value of tag if it exists and is the same in each meta on list */
{
char *val = NULL;
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    char *oneVal = slPairFindVal(stanza->tagList, tag);
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

static void rHoist(struct tagStorm *tagStorm, struct tagStanza *stanza)
/* Move tags that are the same in all children up to parent. */
{
/* Do depth first recursion, but get early return if we're a leaf. */
struct tagStanza *child;
if (stanza->children == NULL)
    return;
for (child = stanza->children; child != NULL; child = child->next)
    rHoist(tagStorm, child);

/* Build up list of tags used in any child. */
struct slName *tag, *tagList = tagsInAny(stanza->children);

/* Go through list and figure out ones that are same in all children. */
for (tag = tagList; tag != NULL; tag = tag->next)
    {
    char *val;
    val = allSameVal(tag->name, stanza->children);
    if (val != NULL)
	hoistOne(tagStorm, stanza, tag->name, val);
    }
slFreeList(&tagList);
}



void tagStormHoist(char *input, char *output)
/* tagStormHoist - Move tags that are shared by all siblings up a level. */
{
struct tagStorm *tagStorm = tagStormFromFile(input);
struct tagStanza *stanza;
for (stanza = tagStorm->forest; stanza != NULL; stanza = stanza->next)
    rHoist(tagStorm, stanza);
tagStormWrite(tagStorm, output, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tagStormHoist(argv[1], argv[2]);
return 0;
}
