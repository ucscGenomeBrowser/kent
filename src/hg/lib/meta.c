/* metaRa - stuff to parse and interpret a genome-hub meta.txt file, which is in 
 * a hierarchical ra format.  That is something like:
 *     meta topLevel
 *     cellLine HELA
 *
 *         meta midLevel
 *         target H3K4Me3
 *         antibody abCamAntiH3k4me3
 *       
 *            meta lowLevel
 *            fileName hg19/chipSeq/helaH3k4me3.narrowPeak.bigBed
 * The file is interpreted so that lower level stanzas inherit tags from higher level ones.
 */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "errabort.h"
#include "meta.h"
#include "ra.h"

int countLeadingSpacesDetabbing(char *s, int tabStop)
/* Count up leading chars including those implied by tab. Set tabStop to 8
 * for usual UNIX results. */
{
int count = 0;
char c;
while ((c = *s++) != 0)
    {
    if (c == ' ')
        ++count;
    else if (c == '\t')
        {
	int tabBefore = (count % tabStop) * tabStop;
	count = (tabBefore+1)*tabStop;
	}
    else
        break;
    }
return count;
}

struct meta *metaNextStanza(struct lineFile *lf)
/* Return next stanza in a meta file.  Does not set parent/child/next pointers. 
 * Returns NULL at end of file.  Does a little error checking,  making sure
 * that indentation level is consistent across all lines of stanza. Returns
 * indentation level. */
{
/* See if anything left in file, and if not return. */
if (!raSkipLeadingEmptyLines(lf, NULL))
    return NULL;

/* Allocate return structure and vars to help parse. */
struct meta *meta;
AllocVar(meta);
struct dyString *dy = dyStringNew(256);
char *tag,*val;

/* Loop to get all tags in stanza. */
boolean firstTime = TRUE;
int initialIndent = 0;
for (;;)
    {
    dyStringClear(dy);
    if (!raNextTagVal(lf, &tag, &val, dy))
        break;

    /* Make tag/val and add it to list. */
    struct metaTagVal *mtv;
    AllocVar(mtv);
    mtv->tag = cloneString(tag);
    mtv->val = cloneString(val);
    slAddHead(&meta->tagList, mtv);

    /* Check indentation. */
    int indent =  countLeadingSpacesDetabbing(dy->string, 8);
    if (firstTime)
        {
	initialIndent = indent;
	firstTime = FALSE;
	}
    else
        {
	if (indent != initialIndent)
	    {
	    warn("Error line %d of %s\n", lf->lineIx, lf->fileName);
	    warn("Indentation level %d doesn't match level %d at start of stanza.", 
		indent, initialIndent);
	    if (strchr(dy->string, '\t'))
	        warn("There are tabs in the indentation, be sure tab stop is set to 8 spaces."); 
	    noWarnAbort();
	    }
	}
    }
slReverse(&meta->tagList);

/* Set up remaining fields and return. */
assert(meta->tagList != NULL);
meta->name = meta->tagList->val;
meta->indent = initialIndent;
return meta;
}

static struct meta *rReverseMetaList(struct meta *list)
/* Return reverse list, and reverse all childen lists too.  Needed because
 * we addHead instead of addTail while building tree because it's faster,
 * especially as lists get long. */
{
slReverse(&list);
struct meta *meta;
for (meta = list; meta != NULL; meta = meta->next)
    {
    if (meta->children != NULL)
        meta->children = rReverseMetaList(meta->children);
    }
return list;
}

struct meta *metaLoadAll(char *fileName, char *keyTag, char *parentTag,
    boolean ignoreOtherStanzas, boolean ignoreIndent)
/* Loads in all ra stanzas from file and turns them into a list of meta, some of which
 * may have children.  The keyTag parameter is optional.  If non-null it should be set to
 * the tag name that starts a stanza.   If null, the first tag of the first stanza will be used.
 * The parentTag if non-NULL will be a tag name used to define the parent of a stanza.
 * The ignoreOtherStanzas flag if set will ignore stanzas that start with other tags.  
 * If not set the routine will abort on such stanzas.  The ignoreIndent if set will
 * use the parentTag (which must be set) to define the hierarchy.  Otherwise the program
 * will look at the indentation, and if there is a parentTag complain about any
 * disagreements between indentation and parentTag. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct meta *meta, *forest = NULL, *lastMeta = NULL;
if (ignoreIndent)
    {
    errAbort("Currently metaLoadAll can't ignore indentation, sorry.");
    }
while ((meta = metaNextStanza(lf)) != NULL)
    {
    struct meta **pList;
    if (forest == NULL)   /* First time. */
        {
	if (meta->indent != 0)
	    errAbort("Initial stanza of %s should not be indented", fileName);
	if (keyTag == NULL)
	    keyTag = meta->tagList->tag;
	pList = &forest;
	}
    else
        {
	if (!sameString(keyTag, meta->tagList->tag))
	    {
	    if (ignoreOtherStanzas)
	        {
		continue;
		// TODO: We should really free memory here. 
		}
	    else
	        errAbort("Stanza beginning with %s instead of %s line %d of %s",
		    meta->tagList->tag, keyTag, lf->lineIx, lf->fileName);
	    }
	if (meta->indent > lastMeta->indent)
	    {
	    pList = &lastMeta->children;
	    meta->parent = lastMeta;
	    }
	else if (meta->indent == lastMeta->indent)
	    {
	    if (meta->indent == 0)
		pList = &forest;
	    else
		{
	        pList = &lastMeta->parent->children;
		meta->parent = lastMeta->parent;
		}
	    }
	else /* meta->indent < lastMeta->indent */
	    {
	    /* Find sibling at same level as us. */
	    struct meta *olderSibling;
	    for (olderSibling = lastMeta->parent; 
		olderSibling != NULL; olderSibling = olderSibling->parent)
	        {
		if (meta->indent == olderSibling->indent)
		    break;
		}
	    if (olderSibling == NULL)
	        {
		warn("Indentation inconsistent in stanza ending line %d of %s.", 
		    lf->lineIx, lf->fileName);
		warn("If you are using tabs, check your tab stop is set to 8.");
		warn("Otherwise check that when you are reducing indentation in a stanza");
		warn("that it is the same as the previous stanza at the same level.");
		noWarnAbort();
		}
	    if (olderSibling->parent == NULL)
	        pList = &forest;
	    else
	        {
		pList = &olderSibling->parent->children;
		meta->parent = olderSibling->parent;
		}
	    }
	}
    slAddHead(pList, meta);
    lastMeta = meta;
    }
lineFileClose(&lf);
forest = rReverseMetaList(forest);
return forest;
}

static void rMetaListWrite(struct meta *metaList, struct meta *parent,
    boolean level, int indent, boolean withParent, FILE *f)
/* Write out list of stanzas at same level to file,  their children too. */
{
int totalIndent = level * indent;
struct meta *meta;
for (meta = metaList; meta != NULL; meta = meta->next)
    {
    struct metaTagVal *mtv;
    boolean gotParent = FALSE;
    for (mtv = meta->tagList; mtv != NULL; mtv = mtv->next)
	{
	if (sameString(mtv->tag, "parent"))
	    {
	    if (withParent)
	        gotParent = TRUE;
	    else
	        continue;   
	    }
	spaceOut(f, totalIndent);
	fprintf(f, "%s %s\n", mtv->tag, mtv->val);
	}
    if (withParent && !gotParent && parent != NULL)
        {
	spaceOut(f, totalIndent);
	fprintf(f, "%s %s\n", "parent", parent->name);
	}
    fprintf(f, "\n");
    if (meta->children)
        rMetaListWrite(meta->children, meta, level+1, indent, withParent, f);
    }
}

void metaWriteAll(struct meta *metaList, char *fileName, int indent, boolean withParent)
/* Write out metadata, optionally adding meta tag. */
{
FILE *f = mustOpen(fileName, "w");
rMetaListWrite(metaList, NULL, 0, indent, withParent, f);
carefulClose(&f);
}

char *metaLocalTagVal(struct meta *meta, char *name)
/* Return value of tag found in this node, not going up to parents. */
{
struct metaTagVal *mtv;
for (mtv = meta->tagList; mtv != NULL; mtv = mtv->next)
    if (sameString(mtv->tag, name))
        return mtv->val;
return NULL;
}

char *metaTagVal(struct meta *meta, char *name)
/* Return value of tag found in this node or if its not there in parents.
 * Returns NULL if tag not found. */
{
struct meta *m;
for (m = meta; m != NULL; m = m->parent)
    {
    char *val = metaLocalTagVal(m, name);
    if (val != NULL)
       return val;
    }
return NULL;
}

static void rHashMetaList(struct hash *hash, struct meta *list)
/* Add list, and any children of list to hash */
{
struct meta *meta;
for (meta = list; meta != NULL; meta = meta->next)
    {
    hashAddUnique(hash, meta->name, meta);
    if (meta->children)
        rHashMetaList(hash, meta->children);
    }
}

struct hash *metaHash(struct meta *forest)
/* Return hash of meta at all levels of heirarchy keyed by forest. */
{
struct hash *hash = hashNew(0);
rHashMetaList(hash, forest);
return hash;
}
