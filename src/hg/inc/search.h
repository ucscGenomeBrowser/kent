// Search code which is shared between different CGIs: hgFileSearch and hgTracks(Track Search)

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef SEARCH_H
#define SEARCH_H

#include "common.h"
#include "cart.h"

// UNCOMMENT TRACK_SEARCH to turn on 'Track Search' functionality
#define TRACK_SEARCH             "hgt_tSearch"
#define TRACK_SEARCH_PAGER       "hgt_tsPage"
#define TRACK_SEARCH_ADD_ROW     "hgt_tsAddRow"
#define TRACK_SEARCH_DEL_ROW     "hgt_tsDelRow"
#define TRACK_SEARCH_BUTTON      "track search"
#define TRACK_SEARCH_HINT        "Search for tracks in this genome assembly"

#define ANYLABEL                 "Any"

#define METADATA_NAME_PREFIX     "hgt_mdbVar"
#define METADATA_VALUE_PREFIX    "hgt_mdbVal"

// Currently selected tab
enum searchTab
    {
    simpleTab   = 0,
    advancedTab = 1,
    filesTab    = 2,
    };


void getSearchTrixFile(char *database, char *buf, int len);
// Fill-in the name of the track search trix file

boolean isSearchTracksSupported(char *database, struct cart *cart);
// Return TRUE if searchTracks is supported for this database and javascript is supported too

struct slPair *fileFormatSearchWhiteList(void);
// Gets the whitelist of approved file formats that is allowed for search

char *fileFormatSelectHtml(char *name, char *selected, char *event, char *javascript, char *style);
// returns an allocated string of HTML for the fileType select drop down

struct slPair *mdbSelectPairs(struct cart *cart, struct slPair *mdbVars);
// Returns the current mdb  vars and vals in the table of drop down selects

char *mdbSelectsHtmlRows(struct sqlConnection *conn,struct slPair *mdbSelects,
                         struct slPair *mdbVars,int cols,boolean fileSearch);
// generates the html for the table rows containing mdb var and val selects.
// Assume tableSearch unless fileSearch

boolean searchNameMatches(struct trackDb *tdb, struct slName *wordList);
// returns TRUE if all words in preparsed list matches short or long label
// A "word" can be "multiple words" (parsed from quoteed string).

boolean searchDescriptionMatches(struct trackDb *tdb, struct slName *wordList);
// returns TRUE if all words in preparsed list matches html description page.
// A "word" can be "multiple words" (parsed from quoteed string).
// Because description contains html, quoted string match has limits.
// DANGER: this will alter html of tdb struct (replacing \n with ' ',
//         so the html should not be displayed after.

#endif /* SEARCH_H */
