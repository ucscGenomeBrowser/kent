/* check gbIndex entries */

/* Copyright (C) 2003 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef CHKGBINDEX_H
#define CHKGBINDEX_H

struct gbSelect;
struct metaDataTbls;

void chkGbIndex(struct gbSelect* select, struct metaDataTbls* metaDataTbls);
/* Check a single partationing of genbank or refseq gbIndex files,
 * checking internal consistency and add to metadata */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
