/* Process an AGP file */

struct agp {
    /* represents a line of an AGP file -- either a fragment or a gap */
    struct agp *next;
    bool isFrag;
    void *entry;        /* agpFrag or agpGap */
} agp;

struct agp *agpLoad(char **row, int ct);
/* Load an AGP entry from array of strings.  Dispose with agpFree */

void agpFree(struct agp **pAgp);
/* Free a dynamically allocated AGP entry */

struct hash *agpLoadAll(char *agpFile);
/* load AGP entries into a hash of AGP lists, one per chromosome */

void agpAddAll(char *agpFile, struct hash *agpHash);
/* add AGP entries from a file into a hash of AGP lists */
