#ifndef MYVARIANTS_H
#define MYVARIANTS_H

#include "jksql.h"
#include "asParse.h"
#include "common.h"
#include "cart.h"
#include "customTrack.h"
#include "myVariantsProject.h"
#define MYVARIANTS_NUM_COLS 21 /* number of default columns before any user-added custom columns */

/* Track-name and type identifiers for myVariants custom tracks.  A project's
 * track name is its SQL table name with the database qualifier removed, so the
 * track says which table it reads and nothing has to be decoded. */
#define MYVARIANTS_TYPE "myVariants"
#define MYVARIANTS_TRACK_PREFIX "myVariants_"
#define MYVARIANTS_SHARED_TRACK_PREFIX "myVariants_shared_"

/* Track group holding the user's annotation tracks, one per project, so a
 * project's name is enough of a label and the tracks are not mixed in with
 * the generic Custom Tracks group.  hgTracks synthesizes the group; the tracks
 * myVariantsAddTracks builds name it in tdb->grp. */
#define MYVARIANTS_GROUP_NAME "myAnnotations"
#define MYVARIANTS_GROUP_LABEL "My Annotations"
/* Puts the group just below Custom Tracks.  hgTracks and the group list the table
 * browsers build both place it, so they read the priority from here. */
#define MYVARIANTS_GROUP_PRIORITY 1.6

/* CGI var asking hgTracks to open the create-annotation dialog as soon as the
 * page loads. Used by the hgTrackUi page for a user with no annotations yet.
 * Listed in hgTracks' excludeVars so the dialog does not reopen on every
 * later page load. */
#define MYVARIANTS_ADD_CGI_VAR "myVarAddAnnotation"

/* CGI var asking hgTracks to open the track manager as soon as the page loads.
 * Used by the hgTrackUi page of a track with no annotations on the assembly.
 * Listed in hgTracks' excludeVars like MYVARIANTS_ADD_CGI_VAR. */
#define MYVARIANTS_MANAGE_CGI_VAR "myVarManageTracks"

/* CGI var carrying the term the position box searched for, sent by its Search
 * button so hgTracks can offer to save it.  Listed in hgTracks' excludeVars. */
#define MYVARIANTS_SEARCH_TERM_VAR "myVarSearchTerm"

/* CGI vars driving the track manager.  MYVARIANTS_PROJECT_CGI_VAR names the
 * action: listing the user's projects, creating one, or renaming one.
 * MYVARIANTS_PROJECT_LABEL_VAR carries the name to give a project.  Answered in
 * JSON before the cart shell runs, the way MYVAR_SHARE_CGI_VAR is. */
#define MYVARIANTS_PROJECT_CGI_VAR "myVarProjectCmd"
#define MYVARIANTS_PROJECT_LABEL_VAR "myVarLabel"

/* Cart var remembering the label of the project the user last saved into, so
 * the dialog opens on it and the quick-add paths, which show no dialog, land
 * there instead of somewhere the user has to go looking for. */
#define MYVARIANTS_LAST_PROJECT_VAR "myVarLastProject"

/* CGI vars driving a download of the signed-in user's annotations straight
 * from their table. MYVARIANTS_DOWNLOAD_CGI_VAR selects the scope, either
 * MYVARIANTS_DOWNLOAD_ALL for every assembly or anything else for the db in
 * the request; MYVARIANTS_DOWNLOAD_SEP_VAR picks the field separator, "csv"
 * for a file Excel opens directly, otherwise tab separated.
 * MYVARIANTS_DOWNLOAD_TRACK_VAR, when present, limits the download to the one
 * track it names; without it every track is included. The description page
 * builds these same URLs, so a rename has to reach
 * htdocs/inc/myVariantsDesc.html too. */
#define MYVARIANTS_DOWNLOAD_CGI_VAR "myVarDownload"
#define MYVARIANTS_DOWNLOAD_SEP_VAR "myVarSep"
#define MYVARIANTS_DOWNLOAD_TRACK_VAR "myVarDownloadTrack"
#define MYVARIANTS_DOWNLOAD_ALL "all"

boolean isMyVariantsType(char *type);
/* TRUE if type names the myVariants custom-track type. NULL-safe. */

boolean isMyVariantsTrack(char *trackName);
/* TRUE if trackName is a myVariants custom track (own or shared). NULL-safe. */

boolean isMyVariantsSharedTrack(char *trackName);
/* TRUE if trackName is a myVariants shared custom track. NULL-safe. */

struct myVariants
/* An item in a myVariants type track. */
    {
    struct myVariants *next;  /* Next in singly linked list. */
    unsigned bin;    /* Bin for range index */
    char *chrom;     /* Reference sequence chromosome or scaffold */
    unsigned chromStart;    /* Start position in chromosome */
    unsigned chromEnd;      /* End position in chromosome */
    char *name;     /* Name of item - up to 16 chars */
    unsigned score; /* 0-1000.  Higher numbers are darker. */
    char strand[2]; /* + or - for strand */
    unsigned thickStart;    /* Start of thick part */
    unsigned thickEnd;      /* End position of thick part */
    unsigned itemRgb;   /* RGB 8 bits each as in bed */
    unsigned blockCount;    /* Number of blocks */
    int *blockSizes;        /* Comma separated list of block sizes */
    int *chromStarts;       /* Start positions relative to chromStart */
    char *description;  /* Longer item description */
    char *db;           /* database name of this annotation */
    char *ref;          /* reference allele */
    char *alt;          /* alternate allele */
    char *mouseover;    /* short mouseover text for hover display */
    char *itemType;     /* transcript, snv, or cnv */
    char *cnvType;      /* gnomAD CNV term; empty for snv/transcript */
    unsigned id;        /* Unique ID for item */
    struct slPair *customFields; /* user-defined custom field name/value pairs */
    };

void myVariantsStaticLoad(char **row, struct myVariants *ret);
/* Load a row from myVariants table into ret. The contents of ret will be replaced at the next call to this function. */

struct myVariants *myVariantsLoadByQuery(struct sqlConnection *conn, char *query);
/* Load all myVariants from table that satisfy the query given. Dispose of this with myVariantsFreeList(). */

void myVariantsSaveToDb(struct sqlConnection *conn, struct myVariants *el, char *tableName, int updateSize);
/* Save myVariants as a row to the table specified by tableName.
 * Uses explicit column names so custom fields in el->customFields are included.
 * If el->name is NULL or empty, fills it in post-INSERT as "Annotation N" using
 * the row's auto-increment id; sqlLastAutoId wraps MariaDB's mysql_insert_id,
 * which is per-connection and unaffected by concurrent INSERTs on other
 * connections. */

struct myVariants *myVariantsLoad(char **row);
/* Load a myVariants from row fetched with select * from myVariants from database. Dispose of this with myVariantsFree(). */

struct myVariants *myVariantsLoadAll(char *fileName);
/* Load all myVariants from whitespace-separated file. Dispose of this with myVariantsFreeList(). */

struct myVariants *myVariantsLoadAllByChar(char *fileName, char chopper);
/* Load all myVariants from chopper separated file. Dispose of this with myVariantsFreeList(). */

#define myVariantsLoadAllByTab(a) myVariantsLoadAllByChar(a, '\t');
/* Load all myVariants from tab separated file. Dispose of this with myVariantsFreeList(). */

struct myVariants *myVariantsCommaIn(char **pS, struct myVariants *ret);
/* Create a myVariants out of a comma separated string. This will fill in ret if non-null, otherwise will return a new myVariants */

void myVariantsFree(struct myVariants **pEl);
/* Free a single dynamically allocated myVariants such as created with myVariantsLoad(). */

void myVariantsFreeList(struct myVariants **pList);
/* Free a list of dynamically allocated myVariants's */

void myVariantsOutput(struct myVariants *el, FILE *f, char sep, char lastSep);
/* Print out myVariants. Separate fields with sep. Follow last field with lastSep. */

#define myVariantsTabOut(el,f) myVariantsOutput(el,f,'\t','\n');
#define myVariantsCommaOut(el,f) myVariantsOutput(el,f,',',',');

struct asObject *myVariantsAsObj();
/* Return asObject describing fields of myVariants */

extern char *myVariantsItemTypes[];
extern int myVariantsNumItemTypes;
extern char *myVariantsCnvTypes[];
extern int myVariantsNumCnvTypes;

char *myVariantsCanonicalItemType(char *s);
/* Return the matching canonical entry from myVariantsItemTypes (case-insensitive),
 * or NULL if s is empty or not in the allow-list. */

char *myVariantsCanonicalCnvType(char *s);
/* Return the matching canonical entry from myVariantsCnvTypes (case-insensitive),
 * or NULL if s is empty or not in the allow-list. */

char *myVariantsGetDatabaseForUser(char *userName);
/* Hash the userName and map it to 1..31 inclusive for deciding what
 * database the table should be created in */

struct myVariantsProject *myVariantsNewProject(char *userName, char *label,
    boolean *retCreated);
/* Create a project for userName: a new annotation table plus the registry row
 * that names it.  The table is created first, so a request that dies part way
 * leaves a table no row points at, never a row pointing at a table that is not
 * there.  Returns the existing project when the label is already taken, so a
 * user cannot end up with two tracks they cannot tell apart.  Returns NULL if
 * userName is empty, label is empty or only whitespace, or the label is too
 * long.  retCreated, when not NULL, is set TRUE only for a project this call
 * made, so a caller that means to alter the new project's table does not alter
 * one the user already had.  Caller frees with myVariantsProjectFreeList. */

struct myVariantsProject *myVariantsListProjects(char *userName);
/* Return every project this user owns, oldest first.  NULL if none, or if the
 * registry table is not installed.  Caller frees with
 * myVariantsProjectFreeList. */

struct myVariantsProject *myVariantsDefaultProject(char *userName);
/* Return the project a save with no project chosen goes into: the user's oldest
 * one, or a new project named MYVARIANTS_DEFAULT_PROJECT when they have none.
 * Every annotation belongs to a project, so this is what the quick-add paths
 * and a first-time user get.  Caller frees. */

void myVariantsForgetProjects();
/* Drop the cached project list, so the next call reads it again.  Anything that
 * creates, renames or removes a project calls this. */

void myVariantsGetFieldLists(char *dbTable, struct slName **retCustom,
    struct slName **retHidden);
/* Split one project's user-added columns into the visible ones and the hidden
 * ones, with the _hidden_ prefix stripped from the latter.  Both lists come off
 * a single description of the table.  Either return may be NULL if the caller
 * does not want that list.  Caller slFreeLists what it asked for. */

struct myVariantsProject *myVariantsProjectForUser(char *userName, unsigned id);
/* Return the project with this id if userName owns it, otherwise NULL.  This is
 * the ownership check for a project id that came in from a form or a URL.
 * Caller frees with myVariantsProjectFreeList. */

struct myVariantsProject *myVariantsFindProject(char *userName, char *trackName);
/* Return the project userName owns that draws as trackName, or NULL when
 * trackName is not one of theirs.  This is the ownership check for an own
 * track: a track name that does not come back is one the caller may not read.
 * Caller frees. */

boolean myVariantsRenameProject(unsigned id, char *userName, char *label);
/* Set a project's label.  userName must own it, and the label must not be empty
 * or only whitespace.  Returns TRUE if a row was updated.  The table and the
 * track name are untouched, so visibility and the other per-track cart
 * variables survive a rename. */

boolean myVariantsDropProject(unsigned id, char *userName);
/* Delete a project: its share, its registry row, then its table, in that
 * order, so a request that dies part way leaves a table no row points at.
 * userName must own the project.  Returns TRUE if it was deleted. */

int myVariantsCountForDb(char *dbTable, char *targetDb);
/* Return how many annotations dbTable holds on targetDb.  0 if the table is
 * gone, which is how a project whose table was swept shows up. */


struct myVariantsShare;
struct myVariantsShare *myVariantsResolveSharedTrack(char *trackName, struct cart *cart);
/* For a "myVariants_shared_*" custom-track name, look up and revalidate the
 * share record from hgcentral.  Returns NULL if the track is not a shared
 * track, the cart does not hold the token, or the share has been revoked.  The
 * returned share carries the validated projectId and permission; callers
 * should use these, not any cart-supplied value, for authorization and
 * scoping.  Caller frees with myVariantsShareFree.
 *
 * The token is the credential: anyone holding it may view the project, and may
 * edit it when the share says so.  The cart variable is the record that this
 * session was given the token, which is why cartLoadUserSession drops these
 * variables when a user loads somebody else's saved session - a session must
 * not pass the credential on. */

char *myVariantsShareDbTable(struct myVariantsShare *share);
/* Return the fully qualified table the share's project is stored in, or NULL
 * if the project is gone.  Caller frees. */

char *myVariantsScopeWhere(char *database, char *trackName);
/* Return a SQL WHERE-clause fragment limiting a myVariants query to the rows a
 * page on database should see.  A project's table holds its annotations on
 * every assembly, so this is the assembly clause; the project itself is the
 * table, and needs no clause.  Returns NULL when there is nothing to add.
 * Caller frees. */

void myVariantsStripHiddenFields(struct slName **pFieldList);
/* Remove any field whose name starts with "_hidden_" from the list in place.
 * Handles bare names ("_hidden_foo") and dotted/qualified names
 * ("db.table._hidden_foo") by checking the segment after the last '.'.
 * Used by hgTables for myVariants_shared_* tables so a recipient does not
 * see columns the owner has hidden. (Custom non-hidden columns are kept,
 * since they are part of the data the owner intentionally shared.) */

boolean myVariantsIsHiddenField(char *table, char *field);
/* TRUE if table is a myVariants table, bare or as db.table, and field is one
 * of its "_hidden_" columns.  NULL-safe. */

void myVariantsAddTracks(struct customTrack **pCtList, struct cart *cart, char *genomeDb);
/* Append the signed-in user's My Annotations tracks for genomeDb, one per
 * project that has annotations there, plus one per share accepted into this
 * cart.  Replaces the ct file the feature used to write and parse back on
 * every request: the registry is the only copy of a project's name and table,
 * so there is nothing a file would add. */

boolean myVariantsHandleCtRemoval(struct customTrack *ct, struct cart *cart,
                                  char *database);
/* Handle "Remove custom track" on a My Annotations track.  For an own track
 * this deletes the project: its share, its registry row and its table.  For a
 * shared track it drops the acceptance so the recipient stops seeing it, and
 * leaves the owner's data alone.  Returns TRUE when ct was a My Annotations
 * track and this function handled the cart cleanup, so the caller must skip
 * its own.  The confirmation is the caller's job; by the time we are here the
 * user has agreed. */

struct slPair *myVariantsGetDbCounts(char *userName);
/* Return a list of assemblies this user has annotations on, each val an item
 * count made with intToPt, summed over all of their projects.  Caller must
 * slPairFreeList the result.  Returns NULL if the user has no annotations. */

struct slName *myVariantsGetCustomFields(char *dbTable);
/* Return list of user-added custom column names on one project's table.
 * Excludes built-in columns and _hidden_ prefixed columns.  Custom fields are
 * per project, so each project has its own list.
 * Caller must slFreeList the result. Returns NULL if no custom fields or table doesn't exist. */

#endif /* MYVARIANTS_H */
