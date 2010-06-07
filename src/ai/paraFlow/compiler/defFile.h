/* defFile - create a file containing the things defined
 * in this file. */

#ifndef DEFFILE_H
#define DEFFILE_H

void pfMakeDefFile(struct pfCompile *pfc, struct pfParse *module, 
	char *defFile);
/* Write out definition file. */

#endif /* DEFFILE_H */
