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
    struct hash *queryHash;            /* hash of pslQuery objects */
};

struct pslTbl *pslTblNew(char *pslFile);
/* construct a new object, loading the psl file */

void pslTblFree(struct pslTbl **pslTblPtr);
/* free object */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

