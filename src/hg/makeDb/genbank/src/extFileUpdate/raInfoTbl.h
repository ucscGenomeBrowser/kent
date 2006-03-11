/* table of seq extFileInfo from a ra file */
#ifndef RAINFOTBL_H
#define RAINFOTBL_H

struct raInfo
/* info from a genbank metaData ra table on an accession */
{
    char *acc;             /* accession */
    int version;
    unsigned size;         /* Size of sequence in bases. */
    off_t offset;          /* Offset in file. */
    unsigned fileSize;     /* Size in file. */
    unsigned extFileId;    /* file id for fasta */
};

struct raInfoTbl *raInfoTblNew();
/* construct a new raInfoTbl */

void raInfoTblRead(struct raInfoTbl *rit, char *raFile, unsigned cdnaExtId, unsigned pepExtId);
/* read a ra file into the table */

struct raInfo *raInfoTblGet(struct raInfoTbl *rit, char *accVer);
/* look up an entry, or return null */

struct raInfo *raInfoTblMustGet(struct raInfoTbl *rit, char *accVer);
/* look up an entry, or abort if not found */

void raInfoTblFree(struct raInfoTbl **ritPtr);
/* free a raInfoTbl object */

#endif
