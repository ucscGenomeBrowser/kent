/* hCommon.h - routines used by many files in hgap project. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef HCOMMON_H
#define HCOMMON_H

#ifndef HGCOLORS_H
#include "hgColors.h"
#endif

char *hgcName();
/* Relative URL to click processing program. */

char *hgTracksName();
/* Relative URL to browser. */

char *hgTrackUiName();
/* Relative URL to extended track UI. */

char *hgFileUiName();
/* Relative URL to downloladable files UI. */

char *hgTextName();
/* Relative URL to old table browser. */

char *hgTablesName();
/* Relative URL to table browser. */

char *hgVaiName();
/* Relative URL to variant annotation integrator. */

char *hgCustomName();
/* Relative URL to custom tracks manager. */

char *hgCollectionName();
/* Relative URL to composite builder. */

char *hgHubConnectName();
/* Relative URL to track hub manager. */

char *hgSessionName();
/* Relative URL to session manager. */

char *hgPalName();
/* Relative URL to click processing program. */

char *hgVarAnnogratorName();
/* Relative URL to variant annotation integrator program. */

char *hgIntegratorName();
/* Relative URL to annotation integrator program. */

char *hgGeneName();
/* Relative URL to gene details program (hgGene). */

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

int chromToInt(char *s);
/* converts a chrom name chrXX into an integer from 1 to 54.
 *  X = 23 Y = 24 Un = 25 M = 26 random = chr + 26;*/

boolean hIsGisaidServer();
/* Is this the GISAID-customized server? Change for config variable
 * gisaid.server=yes */

boolean hIsGsidServer();
/* Is this the GSID-customized server? Change for config variable
 * gsid.server=yes */

boolean hIsCgbServer();
/* Is this a cancer genome browser server? Change for config variable
 * cgb.server=yes */

void hTableStart();
/* Output a table with solid borders. */

void hTableEnd();
/* Close out table started with hTableStart() */

#define hgDefaultPixWidth 950
#define DEFAULT_PIX_WIDTH "950"
/* if this default pix width is changed, also note relationship in
 * CONTROL_TABLE_WIDTH in hui.h */
#define	hgDefaultLeftLabelWidth	120
#define	hgDefaultGfxBorder	1

/* gisaid selection file option variable names */
#define gisaidSubjList "gisaidTable.gisaidSubjList"
#define gisaidSeqList "gisaidTable.gisaidSeqList"
#define gisaidAaSeqList "gisaidTable.gisaidAaSeqList"

boolean validateGisaidUser();

boolean hDumpStackEnabled(void);
/* is browser.dumpStack enabled?  */

void hDumpStackDisallow(void);
/* prevent any dumping of the stack */

void hDumpStackPushAbortHandler(void);
/* push the stack dump abort handler on the stack if it's enabled.  This should be pushed
 * after the warn handle that will do the actual reporting */

void hDumpStackPopAbortHandler(void);
/* pop the stack dump abort handler from the stack if it's enabled */

void hVaUserAbort(char *format, va_list args);
/* errAbort when a `user' error is detected.  This is an error that comes
 * from user input. This disables the logging stack dumps. */

void hUserAbort(char *format, ...)
/* errAbort when a `user' error is detected.  This is an error that comes
 * from user input. This disables the logging stack dumps. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

boolean hAllowAllTables(void);
/* Return TRUE if hg.conf's hgta.disableAllTables doesn't forbid an 'all tables' menu. */

#endif /* HCOMMON_H */
