/* Get mRNA/EST meta data in ra format */
#ifndef RADATA_H
#define RADATA_H

struct gbSelect;

void raDataOpen(char *outFile);
/* open output file and set options */

void raDataProcessUpdate(struct gbSelect* select);
/* Get meta data for a partition and update.  Partition processed index should
 * be loaded and selected versions flaged. */

void raDataClose();
/* close the output file */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

