/* Object to load the metadata from database and verify */
#ifndef CHKMETADATATBLS_H
#define CHKMETADATATBLS_H

struct metaDataTbls;
struct sqlConnection;
struct gbSelect;
struct gbEntry;

struct metaDataTbls* chkMetaDataTbls(struct gbSelect* select,
                                     struct sqlConnection* conn,
                                     boolean checkExtSeqRecs,
                                     unsigned descOrgCats,
                                     char* gbdbMapToCurrent);
/* load the metadata tables do basic validatation */

void chkMetaDataGbEntry(struct metaDataTbls* metaDataTbls,
                        struct gbSelect* select, struct gbEntry* entry);
/* Check metadata against a gbEntry object */

void chkMetaDataXRef(struct metaDataTbls* metaDataTbls);
/* Verify that data that is referenced in some tables is in all expected
 * tables.  Called after processing all indices */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

