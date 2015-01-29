/* testTagStorm - Test speed of loading tag storms.. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "meta.h"
#include "errAbort.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testTagStorm - Test speed of loading tag storms.\n"
  "usage:\n"
  "   testTagStorm input.ra\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct tagStorm
/* Contents of a tagStorm tree.  Each node is a stanza with tags.  Child nodes
 * inherit the tags from their parents in normal use. */
    {
    struct tagStorm *next;
    char *fileName;   /* Name of file if any that tags were read from */
    struct lm *lm;    /* Local memory pool for everything including this structure */
    struct tagStanza *forest;  /* Contains all highest level stanzas */
    };

struct tagStanza
/* An individul stanza in a tagStorm tree.  Holds tagList and possibly children. */
    {
    struct tagStanza *next;	/* Pointer to next younger sibling. */
    struct tagStanza *children;	/* Pointer to eldest child. */
    struct tagStanza *parent;	/* Pointer to parent. */
    struct slPair *tagList;	/* All tags, including the "meta" one. */
    int indent;			/* Indentation level in file */
    };

static void rReverseStanzaList(struct tagStanza **pList)
/* Reverse order of stanzas, used to compensate for all of the add-heads */
{
slReverse(pList);
struct tagStanza *stanza;
for (stanza = *pList; stanza != NULL; stanza = stanza->next)
    {
    if (stanza->children != NULL)
	rReverseStanzaList(&stanza->children);
    }
}

struct tagStorm *tagStormFromFile(char *fileName)
/* Load up all tags from file.  */
{
int depth = 0, maxDepth = 32;
int indentStack[maxDepth];
indentStack[0] = 0;

/* Open up file first thing.  Abort if there's a problem here. */
struct lineFile *lf = lineFileOpen(fileName, TRUE);

/* Set up local memory pool and put tagStorm base structure in it. */
struct lm *lm = lmInit(0);
struct tagStorm *tagStorm = lmAlloc(lm, sizeof(*tagStorm));
tagStorm->lm = lm;
tagStorm->fileName = lmCloneString(lm, fileName);

struct tagStanza *stanza, *parent = NULL, *lastStanza = NULL;
int currentIndent = 0;
int stanzaCount = 0;
int tagCount = 0;
while (raSkipLeadingEmptyLines(lf, NULL))
    {
    ++stanzaCount;
    char *tag, *val;
    int stanzaIndent, tagIndent;
    lmAllocVar(lm, stanza);
    struct slPair *pairList = NULL, *pair;
    while (raNextTagValWithIndent(lf, &tag, &val, NULL, &tagIndent))
        {
	lmAllocVar(lm, pair);
	pair->name = lmCloneString(lm, tag);
	pair->val = lmCloneString(lm, val);
	
	if (pairList == NULL)	/* If this is first tag of a new stanza check indentation
	                         * and put stanza in appropriate level of hierarchy */
	    {
	    if (tagIndent != currentIndent)
		{
		stanzaIndent = tagIndent;
		if (stanzaIndent > currentIndent)
		    {
		    if (++depth >= maxDepth)
		        errAbort("Tags nested too deep line %d of %s. Max nesting is %d",
			    lf->lineIx, lf->fileName, maxDepth);
		    indentStack[depth] = stanzaIndent;
		    if (lastStanza == NULL)
			errAbort("Initial stanza needs to be non-indented line %d of %s", 
			    lf->lineIx, lf->fileName);
		    parent = lastStanza;
		    }
		else  /* going up */
		    {
		    /* Find stanza in parent chain at same level of indentation.  This
		     * will be an older sibling */
		    struct tagStanza *olderSibling;
		    for (olderSibling = parent; olderSibling != NULL; 
			olderSibling = olderSibling->parent)
		        {
			--depth;
			if (indentStack[depth] == stanzaIndent)
			    break;
			}
		    if (olderSibling == NULL)
		        {
			warn("Indentation inconsistent line %d of %s.", lf->lineIx, lf->fileName);
			warn("If you are using tabs, check your tab stop is set to 8.");
			warn("Otherwise check that when you are reducing indentation in a stanza");
			warn("that it is the same as the previous stanza at the same level.");
			noWarnAbort();
			}
		    parent = olderSibling->parent;
		    }
		currentIndent = tagIndent;
		}
	    if (parent == NULL)
	        slAddHead(&tagStorm->forest, stanza);
	    else
		slAddHead(&parent->children, stanza);
	    stanza->parent = parent;
	    pairList = pair;
	    lastStanza = stanza;
	    }
	else
	    {
	    if (tagIndent != currentIndent)
	         errAbort("Tags in stanza inconsistently indented line %d of %s",
		    lf->lineIx, lf->fileName);
	    slAddHead(&pairList, pair);
	    }
	++tagCount;
	}
    slReverse(&pairList);
    stanza->tagList = pairList;
    }
lineFileClose(&lf);
printf("%d tags in %d stanzas\n", tagCount, stanzaCount);
rReverseStanzaList(&tagStorm->forest);
return tagStorm;
}

void tagStormFree(struct tagStorm **pTagStorm)
/* Free up memory associated with tag storm */
{
struct tagStorm *tagStorm = *pTagStorm;
if (tagStorm != NULL)
    {
    lmCleanup(&tagStorm->lm);
    *pTagStorm = NULL;
    }
}


char *tsVal(struct tagStanza *stanza, char *tag)
/* Return value associated with tag in stanza or any of parent stanzas */
{
while (stanza != NULL)
    {
    char *val = slPairFindVal(stanza->tagList, tag);
    if (val != NULL)
         return val;
    stanza = stanza->parent;
    }
return NULL;
}

static void rAddIndex(struct tagStanza *list, struct hash *hash, char *tag, char *parentVal)
/* Add stanza to hash index if it has a value for tag, or if val is passed in. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    char *val = slPairFindVal(stanza->tagList, tag);
    if (val == NULL)
	val = parentVal;
    if (val != NULL)
	hashAdd(hash, val, stanza);
    if (stanza->children != NULL)
        rAddIndex(stanza->children, hash, tag, val);
    }
}

struct hash *tagStormIndex(struct tagStorm *tagStorm, char *tag)
/* Produce a hash of stanzas containing a tag (or whose parents contain tag
 * keyed by tag value */
{
struct hash *hash = hashNew(0);
rAddIndex(tagStorm->forest, hash, tag, NULL);
return hash;
}

static void rTsWrite(struct tagStanza *list, FILE *f, int depth)
/* Recursively write out list to file */
{
struct tagStanza *stanza;
int indent = depth * 3;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *pair;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        {
	spaceOut(f, indent);
	fprintf(f, "%s %s\n", pair->name, (char*)(pair->val));
	}
    fputc('\n', f);
    rTsWrite(stanza->children, f, depth+1);
    }
}
 
void tagStormWriteAll(struct tagStorm *tagStorm, char *fileName)
/* Write all of tag storm to file */
{
FILE *f = mustOpen(fileName, "w");
rTsWrite(tagStorm->forest, f, 0);
carefulClose(&f);
}

void testTagStorm(char *input, char *output)
{
struct tagStorm *tagStorm = tagStormFromFile(input);
uglyf("%d high level tags in %s\n", slCount(tagStorm->forest), tagStorm->fileName);
struct hash *fileHash = tagStormIndex(tagStorm, "file");
uglyf("Got %d stanzas with file\n", fileHash->elCount);
#ifdef SOON
struct hash *qualityHash = tagStormIndex(tagStorm, "lab_kent_quality");
uglyf("Got %d stanzas with quality\n", qualityHash->elCount);
struct hash *sexHash = tagStormIndex(tagStorm, "sex");
uglyf("Got %d stanzas with sex\n", sexHash->elCount);
struct hash *labHash = tagStormIndex(tagStorm, "lab");
uglyf("Got %d stanzas with lab\n", labHash->elCount);
#endif /* SOON */
tagStormWriteAll(tagStorm, output);
tagStormFree(&tagStorm);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
testTagStorm(argv[1], argv[2]);
return 0;
}
