/* hCommon.h - routines used by many files in hgap project. */

#ifndef HCOMMON_H
#define HCOMMON_H

extern char *hgChromNames[]; /* Array of all human chromosome names (including _randoms) */
int hgChromCount;  /* Size of above array. */

char *hgOfficialChromName(char *chrom);
/* Returns "cannonical" name of chromosome (from hgChromNames) or NULL
 * if not a chromosome. */


boolean hgParseChromRange(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd);
/* Parse something of form chrom:start-end into pieces. */

boolean hgIsChromRange(char *spec);
/* Returns TRUE if spec is chrom:N-M for some human
 * chromosome chrom and some N and M. */

char *hgcName();
/* Relative URL to click processing program. */

char *hgTracksName();
/* Relative URL to browser. */

char *hgcFullName();
/* Absolute URL to click processing program. */

char *hgTracksFullName();
/* Absolute URL to browser. */


void fragToCloneName(char *fragName, char cloneName[128]);
/* Convert fragment name to clone name. */

void fragToCloneVerName(char *fragName, char cloneVerName[128]);
/* Convert fragment name to clone.version name. */

void recNameToFileName(char *dir, char *recName, char *fileName, char *suffix);
/* Convert UCSC style fragment name to name of file for a clone. */

void faRecNameToFaFileName(char *dir, char *recName, char *fileName);
/* Convert fa record name to file name. */

void faRecNameToQacFileName(char *dir, char *recName, char *fileName);
/* Convert fa record name to file name. */

void gsToUcsc(char *gsName, char *ucscName);
/* Convert from 
 *    AC020585.5~1.2 Fragment 2 of 29 (AC020585.5:1..1195)
 * to
 *    AC020585.5_1_2
 */

char *skipChr(char *s);
/* Skip leading 'chr' in string (to get the actual chromosome part). */

boolean hIsFin(char *chrom);
/* Return TRUE if this is a finished chromosome. */

#endif /* HCOMMON_H */
