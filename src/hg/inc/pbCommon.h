/* pbCommon - contains data shared within the pb* family of programs
 * (which are used in building the Proteome Browser and data */

#ifndef PBCOMMON_H
#define PBCOMMON_H

/* define the expected amino acid residues */
#define AA_ALPHABET "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

char *txAccFromTempName(char *tempName);
/* Given name in this.that.acc.version format, return
 * just acc.version. */

void txGeneAccFromId(int id, char acc[16]);
/* Convert ID to accession in uc123ABC format. */

#endif /* PBCOMMON_H */

