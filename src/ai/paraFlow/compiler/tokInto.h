/* tokInto - tokenize main module, and also any modules that it
 * goes into.  Leave results in pfc->moduleList. */

#ifndef TOKINTO_H
#define TOKINTO_H

void pfTokenizeInto(struct pfCompile *pfc, char *baseDir, char *modName);
/* Tokenize module, and any modules that it goes into. */

#endif /* TOKINTO_H */

