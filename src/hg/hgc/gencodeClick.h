/* gencodeClick - click handling for GENCODE tracks */
#ifndef GENCODECLICK_H


bool isNewGencodeGene(struct trackDb *tdb);
/* is this a new-style gencode (>= V7) track, as indicated by
 * the presence of the wgEncodeGencodeVersion setting */

void doGencodeGene(struct trackDb *tdb, char *gencodeId);
/* Process click on a GENCODE annotation. */

#endif
