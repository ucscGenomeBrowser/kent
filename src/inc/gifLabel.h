/* gifLabel - create labels as GIF files. */

int gifLabelMaxWidth(char **labels, int labelCount);
/* Return maximum pixel width of labels.  It's ok to have
 * NULLs in labels array. */

void gifLabelVerticalText(char *fileName, char **labels, int labelCount, 
	int height);
/* Make a gif file with given labels.  This will check to see if fileName
 * exists already, and if so do nothing. */
