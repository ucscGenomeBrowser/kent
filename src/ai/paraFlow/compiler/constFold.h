/* This module turns expressions involving constants into just
 * simple constants.  */

#ifndef CONSTFOLD_H
#define CONSTFOLD_H

void pfConstFold(struct pfCompile *pfc, struct pfParse *pp);
/* Fold constants into simple expressions. */

#endif /* CONSTFOLD_H */
