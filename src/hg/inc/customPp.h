/* customPp - custom track preprocessor.  This handles the line-oriented
 * input/output for the custom track system.  The main services it provides
 * are taking care of references to URLs, which are treated as includes,
 * and moving lines that start with "browser" to a list.
 *
 * Also this allows unlimited "pushBack" of lines, which is handy, since
 * the system will analyse a number of lines of a track to see what format
 * it's in. */

#ifndef CUSTOMPP_H
#define CUSTOMPP_H

struct customPp
/* Custom track preprocessor. */
    {
    struct customPp *next;	 /* Next customPp in list */
    struct lineFile *fileStack;  /* Keep track of files we're included from */
    struct slName *browserLines; /* Lines seen so far that start w/ browser */
    };

struct customPp *customPpNew(struct lineFile *lf);
/* Return customPp on lineFile */

void customPpFree(struct customPp **pCpp);
/* Close files and free up customPp. */

char *customPpNext(struct customPp *cpp);
/* Return next line. */

char *customPpNextReal(struct customPp *cpp);
/* Return next line that's nonempty and non-space. */

void customPpReuse(struct customPp *cpp, char *line);
/* Reuse line.  May be called only once before next customPpNext/NextReal */

struct slName *customPpTakeBrowserLines(struct customPp *cpp);
/* Grab browser lines from cpp, which will no longer have them. */

#endif /* CUSTOMPP_H */

