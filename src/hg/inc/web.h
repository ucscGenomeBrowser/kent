/* web.c - some functions to output HTML code */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef WEB_H
#define WEB_H

#include "cart.h"
#include "dbDb.h"

void webSetStyle(char *style);
/* set a style to add to the header */

void webStartText();
/* output the head for a text page */

void webStart(struct cart *theCart, char *db, char* format,...)
/* output a CGI and HTML header with the given title in printf format */
#if defined(__GNUC__)
__attribute__((format(printf, 3, 4)))
#endif
;

void webStartHeader(struct cart *theCart, char *db, char *header, char* format,...)
/* output a CGI and HTML header with the given title in printf format */
#if defined(__GNUC__)
__attribute__((format(printf, 4, 5)))
#endif
;

void webStartWrapper(struct cart *theCart, char *db, char *format, va_list args, boolean withHttpHeader,
	boolean withLogo);
/* output a CGI and HTML header with the given title in printf format */

void webStartWrapperDetailedArgs(struct cart *theCart, char *db,
	char *headerText, char *format, va_list args, boolean withHttpHeader,
	boolean withLogo, boolean skipSectionHeader, boolean withHtmlHeader);
/* output a CGI and HTML header with the given title in printf format with
 * quite a few options.  See also next: */

void webStartWrapperDetailedNoArgs(struct cart *theCart, char *db,
	char *headerText, char *format, boolean withHttpHeader,
	boolean withLogo, boolean skipSectionHeader, boolean withHtmlHeader);
/* output a CGI and HTML header with the given title in printf format with
 * quite a few options.  Same as above without va_list args */

void webPragmasEtc();
/* Print out stuff that tells people not to cache us, and that we use the
 * usual character set and scripting langauge. (Normally done by webStartWrap) */

void webCirmPragmasEtc();
/* Print out stuff similar to webPragmasEtc (don't cache us, character set, etc.), but
 * use values appropriate for a more modern website (like CIRM). */

void webStartSectionTables();
/* Put up start of nepharious table layout stuff. (Normally done by webStartWrap). */

void webFirstSection(char *title);
/* Put up the first section (normally done by webStartWrap). */

void webNewSection(char* format, ...)
/* create a new section on the web page */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

void webNewEmptySection();
/* create a new section on the web page to maintain table layout */

void webNewSectionHeaderStart();
/* Start a new collapsible section on the web page, with +- control.
   Allows use of jsBeginCollapsibleSection() */

void webNewSectionHeaderEnd();
/* Properly close header of collapsible section on web page */

void webEnd();
/* output the footer of the HTML page */

void webEndSection();
/* Close down a section */

void webEndSectionTables();
/* Finish with section tables (but don't do /BODY /HTML lik
 * webEnd does. */

void webStartGbNoBanner(struct cart *cart, char *db, char *title);
/* Start HTML with new header and footer design by jWest, but no banner */

void webEndGb();
/* End HTML that was started with webStartJWest. */

void webStartJWest(struct cart *cart, char *db, char *title);
/* Start HTML with new banner design by jWest (with modifications). */

void webEndJWest();
/* End HTML that was started with webStartJWest/ */

void webVaWarn(char *format, va_list args);
/* Warning handler that closes off web page. */

boolean webGotWarnings();
/* Return TRUE if webVaWarn has been called. */

void webAbortNoHttpHeader(char* title, char* format, ...)
/* an abort function that outputs a error page. No http header output. */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
;

void webAbort(char* title, char* format, ...)
/* an abort function that outputs a error page */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
;

void printCladeListHtml(char *genome, char *event, char *javascript);
/* Make an HTML select input listing the clades. */

void printGenomeListHtml(char *db, char *event, char *javascript);
/* Prints to stdout the HTML to render a dropdown list
 * containing a list of the possible genomes to choose from.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.
 * param event e.g. "change"
 *   javascript - Optional (can be NULL) onEvent javascript. */

void printBlatGenomeListHtml(char *db, char *event, char *javascript);
/* Prints to stdout the HTML to render a dropdown list
 * containing a list of the possible genomes to choose from.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.
 * param event e.g. "change"
 *   javascript - Optional (can be NULL) onEvent javascript. */

void printLiftOverGenomeList(char *customOrgCgiName, char *db,
			     struct dbDb *dbList, char *event, char *javascript);
/* Prints to stdout the HTML to render a dropdown list
 * containing a list of the possible genomes to choose from.
 * Databases in dbList do not have to exist.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.
 * param event e.g. "change"
 *   javascript - Optional (can be NULL) onEvent javascript. */

void printSomeGenomeListHtmlNamed(char *customOrgCgiName, char *db, struct dbDb *dbList, char *event, char *javascript);
/* Prints to stdout the HTML to render a dropdown list
 * containing a list of the possible genomes to choose from.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.
 * param event e.g. "change"
 *   javascript - Optional (can be NULL) onEvent javascript. */

void printSomeGenomeListHtml(char *db, struct dbDb *dbList, char *event, char *javascript);
/* Prints to stdout the HTML to render a dropdown list
 * containing a list of the possible genomes to choose from.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.
 * param event e.g. "change"
 *   javascript - Optional (can be NULL) onEvent javascript. */

void printGenomeListForCladeHtml(char *db, char *event, char *javascript);
/* Prints to stdout the HTML to render a dropdown list containing
 * a list of the possible genomes from selOrganism's clade to choose from.
 * selOrganism is the default for the select.
 */

void webPushErrHandlers();
/* Push warn and abort handler for errAbort(). */

void webPushErrHandlersCartDb(struct cart *cart, char *db);
/* Push warn and abort handler for errAbort(); save cart and db for use in handlers. */

void webPopErrHandlers();
/* Pop warn and abort handler for errAbort(). */

void printAssemblyListHtml(char *db, char *event, char *javascript);
/*
Prints to stdout the HTML to render a dropdown list containing a list of the possible
assemblies to choose from.

param curDb - The assembly (the database name) to choose as selected.
If NULL, no default selection.
 */

void printAssemblyListHtmlExtra(char *db, char *event, char *javascript);
/*
Prints to stdout the HTML to render a dropdown list containing a list of the possible
assemblies to choose from.

param curDb - The assembly (the database name) to choose as selected.
If NULL, no default selection.
param javascript - The javascript text for the select box
 */

void printSomeAssemblyListHtml(char *db, struct dbDb *dbList, char *event, char *javascript);
/* Find all assemblies from the list that are active, and print
 * HTML to render dropdown list
 * param db - default assembly.  If NULL, no default selection */

void printSomeAssemblyListHtmlNamed(char *name, char *db, struct dbDb *dbList, char *onChangeText);
/* Find all assemblies from the list that are active, and print
 * HTML to render dropdown list
 * param db - default assembly.  If NULL, no default selection */

void printAllAssemblyListHtmlParm(char *db, struct dbDb *dbList,
                            char *dbCgi, bool allowInactive, char *event, char *javascript);
/* Prints to stdout the HTML to render a dropdown list containing the list
 * of assemblies for the current genome to choose from.  By default,
 * this includes only active assemblies with a database (with the
 * exception of the default assembly, which will be included even
 * if it isn't active).
 *  param db - The default assembly (the database name) to choose as selected.
 *             If NULL, no default selection.
 *  param allowInactive - if set, print all assemblies for this genome,
 *                        even if they're inactive or have no database
 */

void printSomeAssemblyListHtmlParm(char *db, struct dbDb *dbList,
                                        char *dbCgi, char *event, char *javascript);
/* Find all the assemblies from the list that are active.
Prints to stdout the HTML to render a dropdown list containing the list
of the possible assemblies to choose from.

param db - The default assembly (the database name) to choose as selected.
                If NULL, no default selection.
 */

void printOrgAssemblyListAxtInfo(char *dbCgi, char *javascript);
/* Find all the organisms/assemblies that are referenced in axtInfo,
 * and print the dropdown list. */

void printBlatAssemblyListHtml(char *curDb);
/*
Prints to stdout the HTML to render a dropdown list containing a list of the possible
assemblies to choose from that have blat servers available.

param curDb - The assembly (the database name) to choose as selected.
If NULL, no default selection.
 */

void getDbGenomeClade(struct cart *cart, char **retDb, char **retGenome,
		      char **retClade, struct hash *oldVars);
/* Examine CGI and cart variables to determine which db, genome, or clade
   has been selected, and then adjust as necessary so that all three are
   consistent.  Detect changes and reset db-specific cart variables.
   Save db, genome and clade in the cart so it will be consistent hereafter.
  The order of preference here is as follows:
If we got a request that explicitly names the db, that takes
highest priority, and we synch the organism to that db.
If we get a cgi request for a specific organism then we use that
organism to choose the DB.  If just clade, go from there.

In the cart only, we use the same order of preference.
If someone requests an organism we try to give them the same db as
was in their cart, unless the organism doesn't match.
*/

void getDbAndGenome(struct cart *cart, char **retDb, char **retGenome,
		    struct hash *oldVars);
/* Like getDbGenomeClade above, but just get db and genome. */

void webIncludeFile(char *file);
/* Include an HTML file in a CGI.
 *   The file path may begin with hDocumentRoot(); if it doesn't, it is
 *   assumed to be relative and hDocumentRoot() will be prepended. */

void webIncludeHelpFile(char *fileRoot, boolean addHorizLine);
/* Given a help file root name (e.g. "hgPcrResult" or "cutters"),
 * print out the contents of the file.  If addHorizLine, print out an
 * <HR> first. */

void webIncludeHelpFileSubst(char *fileRoot, struct cart *cart, boolean addHorizLine);
/* Given a help file root name (e.g. "hgPcrResult" or "cutters"),
 * print out the contents of the file.  If cart is non-NULL, invoke hVarSubstWithCart
 * before printing.  If addHorizLine, print out an <HR> first. */

/*** Inside the "web" pages doing a table that looks good is
 *** hard.  These routines help. ***/

void webPrintLinkTableStart();
/* Print link table start in our colors. */

void webPrintLinkTableEnd();
/* Print link table end in our colors. */

void webPrintLinkCellStart();
/* Print link cell start in our colors. */

void webPrintLinkOutCellStart();
/* Print link cell that goes out of our site. End with
 * webPrintLinkTableEnd. */

void webPrintWideCellStart(int colSpan, char *bgColorRgb);
/* Print link multi-column cell start in our colors. */

void webPrintLinkCellRightStart();
/* Print right-justified cell start in our colors. */

void webPrintLinkCellEnd();
/* Print link cell end in our colors. */

void webPrintLinkCell(char *link);
/* Print link cell in our colors, if links is null, print empty cell */

void webPrintIntCell(int val);
/* Print right-justified int cell in our colors. */

void webPrintDoubleCell(double val);
/* Print right-justified cell in our colors with two digits to right of decimal. */

void webPrintLabelCell(char *label);
/* Print label cell in our colors. */

void webPrintWideLabelCell(char *label, int colSpan);
/* Print label cell over multiple columns in our colors. */

void webPrintWideCenteredLabelCell(char *label, int colSpan);
/* Print label cell over multiple columns in our colors and centered. */

void webPrintLabelCellStart();
/* Print start of wrapper around a label in a table. */

void webPrintLabelCellEnd();
/* Print end of wrapper around a label in a table. */

void webPrintLinkTableNewRow();
/* start a new row */

void webFinishPartialLinkTable(int rowIx, int itemPos, int maxPerRow);
/* Fill out partially empty last row. */

void webFinishPartialLinkOutTable(int rowIx, int itemPos, int maxPerRow);
/* Fill out partially empty last row. */

boolean validateGisaidUser(struct cart *cart);
/* validate if the web user is an authenticated GISAID user */

char *webTimeStampedLinkToResource(char *fileName, boolean wrapInHtml);
// Returns full path of timestamped link to the requested resource file (js, or css).
// If wrapInHtml, then returns link embedded in style or script html. Free after use.
// NOTE: png, jpg and gif should also be supported but are untested.

char *webTimeStampedLinkToResourceOnFirstCall(char *fileName, boolean wrapInHtml);
// If this is the first call, will Return full path of timestamped link to the
 // requested resource file (js, or css).  Free after use.
// else returns NULL.  Useful to ensure multiple references to the same resource file are not made
// NOTE: png, jpg and gif should also be supported but are untested.

boolean webIncludeResourcePrintToFile(FILE * toFile, char *fileName);
// Converts fileName to web Resource link and prints the html reference
// This only prints and returns TRUE on first call for this resource.
// Passing in NULL as the file pointer results in hPrintf call
// The reference will be to a link with timestamp.
#define webIncludeResourceFile(fileName)  webIncludeResourcePrintToFile(NULL,fileName)

void lazarusLives(unsigned long newExpireSeconds);
/* Long running process requests more time */

void setContextSpecificHelp(char *link, char *label);
// Override default behavior for the context specific help link

char *menuBarAddUiVars(char *oldString, char *cgiPrefix, char *uiVars);
/* Look for CGI program calls in oldString, and add session vars hgsid to them */

char *menuBar(struct cart *cart, char *db);
// return HTML for the menu bar (read from a configuration file).

void checkForGeoMirrorRedirect(struct cart *cart);
// Implement Geo/IP based redirection
#endif /* WEB_H */
