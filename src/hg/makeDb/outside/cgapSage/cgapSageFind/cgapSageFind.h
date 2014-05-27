/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CGAPSAGEFIND_H
#define CGAPSAGEFIND_H

#define TAG_SIZE 21

/* Function definitions of the supporting code. */

struct hash *getFreqHash(char *freqFile);
/* Read the frequency file in, and store it in a hash and return that. */

struct hash *getTotTagsHash(char *libsFile);
/* Read in the library file and hash up the total tags. */

void hashElSlPairListFree(struct hashEl **pEl);
/* Free up the list in one of the hashEls. */

int pickApartSeqName(char **pName);
/* Change /path/chr:start-end into chr and return start. */

#endif
