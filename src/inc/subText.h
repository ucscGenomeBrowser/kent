/* subText.h - perform text substitutions. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef SUBTEXT_H
#define SUBTEXT_H

struct subText
/* Structure that holds data for a single substitution to be made.
 * The subText routines work mostly on lists of these. */
    {
    struct subText *next;	/* pointer to next substitution */
    char *in;			/* source side of substitution */
    char *out;			/* dest side of substitution */
    int inSize;			/* length of in string */
    int outSize;		/* length of out string */
    };

struct subText *subTextNew(char *in, char *out);
/* Make new substitution structure. */

void subTextFree(struct subText **pSub);
/* Free a subText. */

void subTextFreeList(struct subText **pList);
/* Free a list of dynamically allocated subText's */

int subTextSizeAfter(struct subText *subList, char *in);
/* Return size string will be after substitutions. */

void subTextStatic(struct subText *subList, char *in, char *out, int outMaxSize);
/* Do substition to output buffer of given size.  Complain
 * and die if not big enough. */

char *subTextString(struct subText *subList, char *in);
/* Return string with substitutions in list performed.  freeMem
 * this string when done. */

#endif /* SUBTEXT_H */

