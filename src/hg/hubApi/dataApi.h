/* dataApi.h - functions for API access to UCSC data resources */

#ifndef DATAAPH_H

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "udc.h"
#include "knetUdc.h"
#include "genbank.h"
#include "trackHub.h"
#include "hgConfig.h"
#include "hCommon.h"
#include "hPrint.h"
#include "bigWig.h"
#include "hubConnect.h"
#include "obscure.h"
#include "errCatch.h"
#include "vcf.h"
#include "bedTabix.h"
#include "bamFile.h"
#include "jsonParse.h"
#include "jsonWrite.h"
#include "chromInfo.h"

#ifdef USE_HAL
#include "halBlockViz.h"
#endif

/*this definition should be over in hg/inc/hubPublic.h but that does not exist*/
struct hubPublic
/* Table of public track data hub connections. */
    {
    struct hubPublic *next;  /* Next in singly linked list. */
    char *hubUrl;	/* URL to hub.ra file */
    char *shortLabel;	/* Hub short label. */
    char *longLabel;	/* Hub long label. */
    char *registrationTime;	/* Time first registered */
    unsigned dbCount;	/* Number of databases hub has data for. */
    char *dbList;	/* Comma separated list of databases. */
    char *descriptionUrl;	/* URL to description HTML */
    };

#define MAX_PATH_INFO 32

/*  functions in hubApi.c */
struct hubPublic *hubPublicLoadAll();

struct dbDb *ucscDbDb();
/* return the dbDb table as an slList */

struct slName *genomeList(struct trackHub *hubTop, struct trackDb **dbTrackList, char *selectGenome);
/* follow the pointers from the trackHub to trackHubGenome and around
 * in a circle from one to the other to find all hub resources
 * return slName list of the genomes in this track hub
 * optionally, return the trackList from this hub for the specified genome
 */

/*  functions in apiUtils.c */
void apiErrAbort(char *format, ...);
/* Issue an error message in json format, and exit(0) */

struct jsonWrite *apiStartOutput();
/* begin json output with standard header information for all requests */

int tableColumns(struct sqlConnection *conn, struct jsonWrite *jw, char *table);
/* output the column names, and their MySQL data type, for the given table
 *  return number of columns (aka 'fields')
 */

/* ######################################################################### */
/*  functions in getData.c */

void apiGetData(char *words[MAX_PATH_INFO]);
/* 'getData' function, words[1] is the subCommand */

/* ######################################################################### */
/*  functions in list.c */

void apiList(char *words[MAX_PATH_INFO]);
/* 'list' function words[1] is the subCommand */

#endif	/*	 DATAAPH_H	*/
