/* ascUtils.h - Routines and data structures for reading and writing
 * alt spliced catalog. */

struct altSpec
/* This structure holds info on one entry of alt-splicing catalog. */
    {
    struct altSpec *next;
    char *hrefTag;
    char *orfName;
    boolean skipExon, skipIntron, nonGenomic;
    int alt3, alt5;
    int ieOverlap, iiOverlap;
    struct slName *cdna;
    boolean hasNewCdna;
    boolean isInOld;
    };

struct altSpec *readAlts(char *fileName, boolean oldStyle);
/* Read alt-splicing file and turn it into an altSpec list. */

void writeAlts(char *fileName, struct altSpec *altList, 
    boolean newOnly, boolean notOldOnly);
/* Write alt-splicing file. */

char *eatHrefTag(char *line);
/* Remove HrefTag from line and return it. */

