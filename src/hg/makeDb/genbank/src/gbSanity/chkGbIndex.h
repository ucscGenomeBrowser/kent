/* check gbIndex entries */
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
