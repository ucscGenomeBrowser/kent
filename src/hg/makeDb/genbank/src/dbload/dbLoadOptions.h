/* option shared by all dbload modules */
#ifndef GBLOADOPTIONS_H
#define GBLOADOPTIONS_H

#define DBLOAD_INITIAL           0x01   /* initial load of this databases */
#define DBLOAD_GO_FASTER         0x02   /* optimize speed over memory */
#define DBLOAD_LARGE_DELETES     0x04   /* don't enforce fraction deleted */
#define DBLOAD_XENO_MRNA_DESC    0x08   /* load descriptions for xeno mRNAs */
#define DBLOAD_XENO_REFSEQS      0x10   /* load xeno refseqs */
#define DBLOAD_PER_CHROM_ALIGN   0x20   /* build per-chromosome alignment tables */
#define DBLOAD_DRY_RUN           0x40   /* Don't modify database */


#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
