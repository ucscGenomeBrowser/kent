/* table of psl alignments, grouped by query */
#ifndef PSLTBL_H
#define PSLTBL_H

struct psl;
struct hash;

struct pslQuery
/* object containing PSLs for a single query */
{
    struct pslQuery *next;
    char *qName;          /* qName, memory not owned here */
    struct psl *psls;     /* alignments */
};

struct pslTbl
/* table of psl alignments */
{
    struct pslTbl *next;               /* next psl table in a list */
    char *setName;                     /* name identifying the set of psl.
                                        * maybe file name, or  other name */
    struct hash *queryHash;            /* hash of pslQuery objects */
};

struct pslTbl *pslTblNew(char *pslFile, char *setName);
/* construct a new object, loading the psl file.  If setName is NULL, the file
* name is saved as the set name. */

void pslTblFree(struct pslTbl **pslTblPtr);
/* free object */

void pslTblFreeList(struct pslTbl **pslTblList);
/* free list of pslTbls */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

