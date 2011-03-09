/* table of accessions and moddates to ignore */
#ifndef GBIGNORE_H
#define GBIGNORE_H

struct gbRelease;

struct gbIgnoreAcc
/* an accession and moddate to ignore, multiple entries for an accession
 * are linked. */
{
    struct gbIgnoreAcc *next;  /* next entry for acc */
    char* acc;
    time_t modDate;
    unsigned srcDb;
};

struct gbIgnore;
/* table of accessions and moddates to ignore */

struct gbIgnore* gbIgnoreLoad(struct gbRelease* release);
/* Load the ignore index.  It is loading into the memory associated with 
 * the release, although it is not specific to the release. */

struct gbIgnore* gbIgnoreNew(struct gbRelease* release);
/* Load the ignore index. */

void gbIgnoreFree(struct gbIgnore** ignorePtr);
/* Free gbIgnore object */

struct gbIgnoreAcc* gbIgnoreGet(struct gbIgnore *ignore, char *acc,
                                time_t modDate);
/* Get he ignore entry for an accession and modedate, or NULL  */

struct gbIgnoreAcc* gbIgnoreFind(struct gbIgnore *ignore, char *acc);
/* get the list of gbIgnore entries for an accession, or NULL */


struct hashCookie gbIgnoreFirst(struct gbIgnore *ignore);
/* get cookie to iterate over hash */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
