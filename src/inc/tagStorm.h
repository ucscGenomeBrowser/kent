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
    };

/** Read and write tag storms from/to files. */

struct tagStorm *tagStormFromFile(char *fileName);
/* Load up all tags from file.  */

void tagStormFree(struct tagStorm **pTagStorm);
/* Free up memory associated with tag storm */

void tagStormWrite(struct tagStorm *tagStorm, char *fileName, int maxDepth);
/* Write all of tag storm to file */

void tagStormWriteAsFlatRa(struct tagStorm *tagStorm, char *fileName, char *idTag, 
    boolean withParent, int maxDepth, boolean leavesOnly);
/* Write tag storm flattening out hierarchy so kids have all of parents tags in .ra format */

void tagStormWriteAsFlatTab(struct tagStorm *tagStorm, char *fileName, char *idTag,
    boolean withParent, int maxDepth, boolean leavesOnly);
/* Write tag storm flattening out hierarchy so kids have all of parents tags in tab-sep format */

#ifdef OLD
char *tagStanzaVal(struct tagStanza *stanza, char *tag);
/* Return value associated with tag in stanza or any of parent stanzas */
#endif /* OLD */

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

void tagStormUpdateTag(struct tagStorm *tagStorm, struct tagStanza *stanza, char *tag, char *val);
/* Add tag to stanza in storm, replacing existing tag if any. If tag is added it's added to
 * end. */

/** Stuff for constructing a tag storm a tag at a time rather than building it from file  */

struct tagStorm *tagStormNew(char *name);
/* Create a new, empty, tagStorm. */

struct tagStanza *tagStanzaNew(struct tagStorm *tagStorm, struct tagStanza *parent);
/* Create a new, empty stanza that is added as to head of child list of parent,
 * or to tagStorm->forest if parent is NULL. */

struct tagStanza *tagStanzaNewAtEnd(struct tagStorm *tagStorm, struct tagStanza *parent);
/* Create a new, empty stanza that is added as to head of child list of parent,
 * or to tagStorm->forest if parent is NULL. */
/* Create new empty stanza that is added at tail of child list of parent */

struct slPair *tagStanzaAdd(struct tagStorm *tagStorm, struct tagStanza *stanza, 
    char *tag, char *val);
/* Add tag with given value to stanza */

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

/** Information about a tag storm */

struct slName *tagStormFieldList(struct tagStorm *tagStorm);
/* Return list of all fields in storm */

struct hash *tagStormFieldHash(struct tagStorm *tagStorm);
/* Return an integer-valued hash of fields, keyed by tag name and with value
 * number of times field is used.  For most purposes just used to make sure
 * field exists though. */

struct hash *tagStormCountTagVals(struct tagStorm *tags, char *tag);
/* Return an integer valued hash keyed by all the different values
 * of tag seen in tagStorm.  The hash is filled with counts of the
 * number of times each value is used that can be recovered with 
 * hashIntVal(hash, key) */

/** Stuff for finding tags within a stanza */

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
