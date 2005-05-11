/* parseInto - Handle parsing across multiple modules. */

#ifndef PARSEINTO_H
#define PARSEINTO_H

struct pfParse *pfParseInto(struct pfCompile *pfc);
/* Parse file.  Also parse .pfh files for any modules the main file goes into.
 * If the .pfh files don't exist then create them. */

#endif /* PARSEINTO_H */
