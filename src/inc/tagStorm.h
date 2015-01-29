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

void tagStormWriteAll(struct tagStorm *tagStorm, char *fileName);
/* Write all of tag storm to file */

char *tagStanzaVal(struct tagStanza *stanza, char *tag);
/* Return value associated with tag in stanza or any of parent stanzas */

struct hash *tagStormIndex(struct tagStorm *tagStorm, char *tag);
/* Produce a hash of stanzas containing a tag (or whose parents contain tag
 * keyed by tag value */

void tagStormAdd(struct tagStorm *tagStorm, struct tagStanza *stanza, char *tag, char *val);
/* Add tag to stanza in storm, replacing existing tag if any */

#endif /* TAGSTORM_H */
