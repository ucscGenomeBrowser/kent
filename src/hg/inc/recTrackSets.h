/* recTrackSets - read the list of recommended track sets for a database. */

/* Copyright (C) 2020 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef RECTRACKSETS_H
#define RECTRACKSETS_H

/* Recommended track sets are Special 'curated' sessions, created by browser team, e.g. for clinical users
 *      This is expected to be a very limited number (under 10 ?)
 *      The list references sessions in namedSessionDb table, by userName and sessionName
 *      (unfortunately not required to be unique, so depending on curator to
 *      just make one (code willl pick the first one)
 */
struct recTrackSet
    {
    struct recTrackSet *next;
    char *label;        // short label for display on browser and dialogs
    char *userName;     // field in named sessions table
    char *sessionName;  // field in named sessions table (CGI encoded)
    char *description;  // descriptive phrase or sentence.  Display uses this
                                // instead of description in session settings to allow
                                // updating by other than session author (e.g. QA)
    };

#define REC_TRACK_SETS_FILE  "recTrackSets"
#define REC_TRACK_SETS_EXT  "tab"
#define REC_TRACK_SETS_DATA_DIR  "data/recTrackSets"

char *recTrackSetsFile(char *db);
/* Generate path to file specifying menu of recommended track sets.
 *      eg, DOCUMENT_ROOT/data/recTrackSets/recTrackSets.hg19.tab */

boolean recTrackSetsEnabled(char *db);
/* Return TRUE if feature is available */

struct recTrackSet *loadRecTrackSets(char *db);
/* Read from tab-sep file.  Return list or NULL if no track sets for this database */

#endif /* RECTRACKSETS_H */
