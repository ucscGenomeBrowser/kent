/* tagStorm - stuff to parse and interpret a genome-hub metadata.txt file, which is in 
 * a hierarchical ra format.  That is something like:
 *     cellLine HELA
 *     lab ucscCore
 *
 *         target H3K4Me3
 *         antibody abCamAntiH3k4me3
 *       
 *            fileName hg19/chipSeq/helaH3k4me3.narrowPeak.bigBed
 *            format narrowPeak
 *
 *            fileName hg19/chipSeq/helaH3K4me3.broadPeak.bigBed
 *            formet broadPeak
 *
 *         target CTCF
 *         antibody abCamAntiCtcf
 *
 *            fileName hg19/chipSeq/helaCTCF.narrowPeak.bigBed
 *            format narrowPeak
 *
 *            fileName hg19/chipSeq/helaCTCF.broadPeak.bigBed
 *            formet broadPeak
 *
 * The file is interpreted so that lower level stanzas inherit tags from higher level ones.
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
    struct slPair *tagList;	/* All tags, including the "meta" one. */
    };

struct tagStorm *tagStormFromFile(char *fileName);
/* Load up all tags from file.  */

void tagStormFree(struct tagStorm **pTagStorm);
/* Free up memory associated with tag storm */

void tagStormWrite(struct tagStorm *tagStorm, char *fileName, int maxDepth);
/* Write all of tag storm to file */

void tagStormWriteAsFlatRa(struct tagStorm *tagStorm, char *fileName, char *idTag, 
    boolean withParent, int maxDepth);
/* Write tag storm flattening out hierarchy so kids have all of parents tags in .ra format */

void tagStormWriteAsFlatTab(struct tagStorm *tagStorm, char *fileName, char *idTag,
    boolean withParent, int maxDepth);
/* Write tag storm flattening out hierarchy so kids have all of parents tags in tab-sep format */

char *tagStanzaVal(struct tagStanza *stanza, char *tag);
/* Return value associated with tag in stanza or any of parent stanzas */

struct hash *tagStormIndex(struct tagStorm *tagStorm, char *tag);
/* Produce a hash of stanzas containing a tag (or whose parents contain tag
 * keyed by tag value */

void tagStormUpdateTag(struct tagStorm *tagStorm, struct tagStanza *stanza, char *tag, char *val);
/* Add tag to stanza in storm, replacing existing tag if any. If tag is added it's added to
 * end. */

/** Stuff for constructing a tag storm a tag at a time rather than building it from file */

struct tagStorm *tagStormNew(char *fileName);
/* Create a new, empty, tagStorm. */

struct tagStanza *tagStanzaNew(struct tagStorm *tagStorm, struct tagStanza *parent);
/* Create a new, empty stanza that is added as to head of child list of parent,
 * or to tagStorm->forest if parent is NULL. */

struct slPair *tagStanzaAdd(struct tagStorm *tagStorm, struct tagStanza *stanza, 
    char *tag, char *val);
/* Add tag with given value to stanza */

void tagStormReverseAll(struct tagStorm *tagStorm);
/* Reverse order of all lists in tagStorm.  Use when all done with tagStanzaNew
 * and tagStanzaAdd (which for speed build lists backwards). */

#endif /* TAGSTORM_H */
