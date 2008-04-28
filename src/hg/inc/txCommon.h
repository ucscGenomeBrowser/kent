/* txCommon - contains routines shared by various programs in
 * the txXxx family (which is used to make the UCSC gene set */

#ifndef TXCOMMON_H
#define TXCOMMON_H

char *txAccFromTempName(char *tempName);
/* Given name in this.that.acc.version format, return
 * just acc.version. */

void txGeneAccFromId(int id, char acc[16]);
/* Convert ID to accession in uc123ABC format. */

#endif /* TXCOMMON_H */

