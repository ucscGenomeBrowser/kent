/* tagStorm - stuff to parse and interpret a genome-hub metadata.txt file, which is in 
 * a hierarchical ra format.  That is something like:
 * 
 *     cellLine HELA
 *     lab ucscCore
 *
 *         target H3K4Me3
 *         antibody abCamAntiH3k4me3
 *       
 *            file hg19/chipSeq/helaH3k4me3.narrowPeak.bigBed
 *            format narrowPeak
 *
 *            file hg19/chipSeq/helaH3K4me3.broadPeak.bigBed
 *            formet broadPeak
 *
 *         target CTCF
 *         antibody abCamAntiCtcf
 *
 *            file hg19/chipSeq/helaCTCF.narrowPeak.bigBed
 *            format narrowPeak
 *
 *            file hg19/chipSeq/helaCTCF.broadPeak.bigBed
 *            format broadPeak
 *
 * The file is broken into stanzas, that are separated from each other by blank lines.  The blank lines
 * can include whitespace, which is ignored.  Multiple blank lines can separate stanzas with no difference
 * in meaning from a single blank line.  Lines within a stanza start with a word which is the field label.
 * A tab or a space (or multiple tabs or spaces) separate the label from the field contents.  The label
 * can't contain spaces.
 *
 * The file is interpreted so that lower level stanzas inherit tags from higher level ones.
 * This file might be used as so:
 *   struct tagStorm *tags = tagStormFromFile("metadata.txt");
 *   struct hash *fileIndex = tagStormUniqueIndex(tags, "file");
 *   struct tagStanza *stanza = hashMustFindVal(fileIndex,"hg19/chipSeq/helaCTCF.broadPeak.bigBed");
 *   char *target = tagFindVal(stanza, "target");	// Target is CTCF
 *
 * Most commonly indentation is done one tab-character per indentation level.  Spaces may also
 * be used.  If spaces and tabs are mixed sometimes you get surprises.  The tab-stop is interpreted
 * as happening every 8 spaces.
 *
 */

#ifndef TAGSTORM_H
#define TAGSTORM_H

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
    struct slPair *tagList;	/* All tags. Best not to count on the order. */
    int startLineIx;		/* Starting line number for stanza, for error reporting */
    };

/** Read and write tag storms from/to files. */

struct tagStorm *tagStormFromFile(char *fileName);
/* Load up all tags from file.  Use tagStormFree on result when done. */

void tagStormFree(struct tagStorm **pTagStorm);
/* Free up memory associated with tag storm */

void tagStormWrite(struct tagStorm *tagStorm, char *fileName, int maxDepth);
/* Write all of tag storm to file */

void tagStormWriteAsFlatRa(struct tagStorm *tagStorm, char *fileName, char *idTag, 
    boolean withParent, int maxDepth, boolean leavesOnly);
/* Write tag storm flattening out hierarchy so kids have all of parents tags in .ra format */

void tagStormWriteAsFlatTab(struct tagStorm *tagStorm, char *fileName, char *idTag, 
    boolean withParent, int maxDepth, boolean leavesOnly, char *nullVal, boolean sharpLabel);
/* Write tag storm flattening out hierarchy so kids have all of parents tags in .tsv format */

void tagStormWriteAsFlatCsv(struct tagStorm *tagStorm, char *fileName, char *idTag, 
    boolean withParent, int maxDepth, boolean leavesOnly, char *nullVal);
/* Write tag storm flattening out hierarchy so kids have all of parents tags in .csv format */

void tagStanzaRecursiveWrite(struct tagStanza *list, FILE *f, int maxDepth, int depth);
/* Recursively write out stanza list and children to open file */

void tagStanzaSimpleWrite(struct tagStanza *stanza, FILE *f, int depth);
/* Write out tag stanza to file.  Do not recurse or add last blank line */

/** Index a tag storm and look up in an index. */

struct hash *tagStormIndex(struct tagStorm *tagStorm, char *tag);
/* Produce a hash of stanzas containing a tag keyed by tag value */

struct hash *tagStormUniqueIndex(struct tagStorm *tagStorm, char *tag);
/* Produce a hash of stanzas containing a tag where tag is unique across
 * stanzas */

struct hash *tagStormIndexExtended(struct tagStorm *tagStorm, char *tag, 
    boolean unique, boolean inherit);
/* Produce a hash of stanzas containing a tag keyed by tag value. 
 * If unique parameter is set then the tag values must all be unique
 * If inherit is set then tags set in parent stanzas will be considered too. */

struct tagStanza *tagStanzaFindInHash(struct hash *hash, char *key);
/* Find tag stanza that matches key in an index hash returned from tagStormUniqueIndex.
 * Returns NULL if no such stanza in the hash.
 * (Just a wrapper around hashFindVal.)  Do not free tagStanza that it returns. For
 * multivalued indexes returned from tagStormIndex use hashLookup and hashLookupNext. */

/** Stuff for constructing a tag storm a tag at a time rather than building it from file  */

struct tagStorm *tagStormNew(char *name);
/* Create a new, empty, tagStorm. */

struct tagStanza *tagStanzaNew(struct tagStorm *tagStorm, struct tagStanza *parent);
/* Create a new, empty stanza that is added as to head of child list of parent,
 * or to tagStorm->forest if parent is NULL. */

struct tagStanza *tagStanzaNewAtEnd(struct tagStorm *tagStorm, struct tagStanza *parent);
/* Create new empty stanza that is added at tail of child list of parent */

struct slPair *tagStanzaAdd(struct tagStorm *tagStorm, struct tagStanza *stanza, 
    char *tag, char *val);
/* Add tag with given value to beginning of stanza */

struct slPair *tagStanzaAppend(struct tagStorm *tagStorm, struct tagStanza *stanza, 
    char *tag, char *val);
/* Add tag with given value to the end of the stanza */

void tagStanzaAddLongLong(struct tagStorm *tagStorm, struct tagStanza *stanza, char *var, 
    long long val);
/* Add long long integer valued tag to stanza */

void tagStanzaAddDouble(struct tagStorm *tagStorm, struct tagStanza *stanza, char *var, 
    double val);
/* Add double valued tag to stanza */

void tagStormReverseAll(struct tagStorm *tagStorm);
/* Reverse order of all lists in tagStorm.  Use when all done with tagStanzaNew
 * and tagStanzaAdd (which for speed build lists backwards). */

/** Stuff for editing a tagStorm */

void tagStanzaUpdateTag(struct tagStorm *tagStorm, struct tagStanza *stanza, char *tag, char *val);
/* Add tag to stanza in storm, replacing existing tag if any. If tag is added it's added to
 * end. */

void tagStanzaDeleteTag(struct tagStanza *stanza, char *tag);
/* Remove a tag from a stanza */

void tagStormRemoveEmpties(struct tagStorm *tagStorm);
/* Remove any empty stanzas, promoting children if need be. */

void tagStormHoist(struct tagStorm *tagStorm, char *selectedTag);
/* Hoist tags that are identical in all children to parent.  If selectedTag is
 * non-NULL, just do it for tag of that name rather than all tags. */

void tagStormAlphaSort(struct tagStorm *tagStorm);
/* Sort tags in stanza alphabetically */

void tagStormOrderSort(struct tagStorm *tagStorm, char **orderFields, int orderCount);
/* Sort tags in stanza to be in same order as orderFields input  which is orderCount long */

void tagStormDeleteTags(struct tagStorm *tagStorm, char *tagName);
/* Delete all tags of given name from tagStorm */

struct slPair *tagStanzaDeleteTagsInHash(struct tagStanza *stanza, struct hash *weedHash);
/* Delete any tags in stanza that have names that match hash. Return list of removed tags. */

void tagStormSubTags(struct tagStorm *tagStorm, char *oldName, char *newName);
/* Rename all tags with oldName to newName */

void tagStanzaSubTagsInHash(struct tagStanza *stanza, struct hash *valHash);
/* Substitute tags that are keys in valHash with the values in valHash */

void tagStanzaRecursiveRemoveWeeds(struct tagStanza *list, struct hash *weedHash);
/* Recursively remove weeds in list and any children in list */

void tagStormWeedArray(struct tagStorm *tagStorm, char **weeds, int weedCount);
/* Remove all tags with names matching any of the weeds from storm */

void tagStanzaRecursiveSubTags(struct tagStanza *list, struct hash *subHash);
/* Recursively remove weeds in list and any children in list */

void tagStormSubArray(struct tagStorm *tagStorm, char *subs[][2], int subCount);
/* Substitute all tag names with substitutions from subs array */

void tagStormCopyTags(struct tagStorm *tagStorm, char *oldTag, char *newTag);
/* Make a newTag that has same value as oldTag every place oldTag occurs */

/** Information about a tag storm */

struct slName *tagStormFieldList(struct tagStorm *tagStorm);
/* Return list of all fields in storm */

struct hash *tagStormFieldHash(struct tagStorm *tagStorm);
/* Return an integer-valued hash of fields, keyed by tag name and with value
 * number of times field is used.  For most purposes just used to make sure
 * field exists though. */

struct hash *tagStormCountTagVals(struct tagStorm *tags, char *tag, char *requiredTag);
/* Return an integer valued hash keyed by all the different values
 * of tag seen in tagStorm.  The hash is filled with counts of the
 * number of times each value is used that can be recovered with 
 * hashIntVal(hash, key).  If requiredTag is not-NULL, stanza must 
 * have that tag. */

int tagStormMaxDepth(struct tagStorm *ts);
/* Calculate deepest extent of tagStorm */

int tagStormCountStanzas(struct tagStorm *ts);
/* Return number of stanzas in storm */

int tagStormCountTags(struct tagStorm *ts);
/* Return number of tags in storm. Does not include expanding ancestors */

int tagStormCountFields(struct tagStorm *ts);
/* Return number of distinct tag types (fields) in storm */

/** Stuff to find stanzas in a tree. */

struct tagStanza *tagStormQuery(struct tagStorm *tagStorm, char *fields, char *where);
/* Returns a list of tagStanzas that match the properties describe in  the where parameter.  
 * The field parameter is a comma separated list of fields that may include wildcards.  
 * For instance "*" will select all fields,  "a*,b*" will select all fields starting with 
 * an "a" or a "b." The where parameter is a Boolean expression similar to what could appear 
 * in a SQL where clause.  Free up the result when you are done with tagStanzaFreeList. */

void tagStanzaFreeList(struct tagStanza **pList);
/* Free up tagStanza list from tagStormQuery. Don't try to free up stanzas from the
 * tagStorm->forest with this though, as those are in a diffent, local, memory pool. */

struct rqlStatement;  // Avoid having to include rql.h

boolean tagStanzaRqlMatch(struct rqlStatement *rql, struct tagStanza *stanza,
	struct lm *lm);
/* Return TRUE if where clause and tableList in statement evaluates true for stanza. */

char *tagStanzaRqlLookupField(void *record, char *key);
/* Lookup a field in a tagStanza for rql. */

/** Apply a function to every stanza in a tag storm or get a list of all leaf stanzas. */

void tagStormTraverse(struct tagStorm *storm, struct tagStanza *stanzaList, void *context,
    void (*doStanza)(struct tagStorm *storm, struct tagStanza *stanza, void *context));
/* Traverse tagStormStanzas recursively applying doStanza with to each stanza in
 * stanzaList and any children.  Pass through context */

struct tagStanzaRef
/* A reference to a stanza in a tag storm */
    {
    struct tagStanzaRef *next;	/* Next in list */
    struct tagStanza *stanza;   /* The stanza */
    };

struct tagStanzaRef *tagStormListLeaves(struct tagStorm *tagStorm);
/* Return list of references to all stanzas in tagStorm.  Free this
 * result with slFreeList. */

/** Stuff for finding tags within a stanza */

/* Find values in a stanza */
char *tagFindLocalVal(struct tagStanza *stanza, char *name);
/* Return value of tag of given name within stanza, or NULL * if tag does not exist. 
 * This does *not* look at parent tags. */

char *tagFindVal(struct tagStanza *stanza, char *name);
/* Return value of tag of given name within stanza or any of it's parents, or NULL
 * if tag does not exist. */

char *tagMustFindVal(struct tagStanza *stanza, char *name);
/* Return value of tag of given name within stanza or any of it's parents. Abort if
 * not found. */

struct slPair *tagListIncludingParents(struct tagStanza *stanza);
/* Return a list of all tags including ones defined in parents. */

#endif /* TAGSTORM_H */
