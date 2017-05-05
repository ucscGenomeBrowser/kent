/* buildReleaseLog - Build an updated release log based on track information pulled from a
 * redmine server. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "kxTok.h"

#include "htmlOut.h"

#define TRACKER_TRACK_NAME "Track"
#define TRACKER_ASSEMBLY_NAME "Assembly"

#define CF_RELEASE_TEXT_NAME "Release Log Text"
#define CF_RELEASE_URL_NAME "Release Log URL"
#define CF_RELEASE_FLAG_NAME "Released to RR"
#define CF_ASSEMBLIES_NAME "Assemblies"

#define DEFAULT_SINCE_DATE "2016-01-01"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "buildReleaseLog - build an HTML release log from redmine data\n"
  "usage:\n"
  "   buildReleaseLog releaseLog.shtml\n"
  "This will build an SHTML release log page using track release information pulled from a\n"
  "redmine server.  Connects to the redmine server specified in the .hg.conf profile \"redmine\"\n"
  "and pulls data on all tracks released since a particular date.  Compiles release details for\n"
  "those tracks and writes the resulting release log to a file with the supplied filename.\n"
  "Note: the resulting release log uses Apache SSI statements to invoke standard UCSC Genome\n"
  "Browser header and footer documents, so those files must be available to view the page on the\n"
  "web.\n"
  "options:\n"
  "    -since=XXX\tAdd data for all tracks created since the specified date.  Format can be\n"
  "            anything accepted as a date by MySQL.  Defaults to "DEFAULT_SINCE_DATE".\n"
  "    -oldLog=XXX\tInclude a link to the specified previous release log file.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"since", OPTION_STRING},
   {"oldLog", OPTION_STRING},
   {NULL, 0},
};

struct ticketCustomFields
/* Extra fields associated with a redmine ticket that are relevant for the log */
    {
    struct ticketCustomFields *next;
    char *releaseText;
    char *releaseUrl;
    struct slName *assemblies;
    char *ticketId;
    };

struct trackEntry
/* Simple object to keep track of, heh, tracks */
    {
    struct trackEntry *next;
    char *ticketId;
    };

struct assemblyEntry
/* dbDb fields used to describe assemblies in the log */
    {
    struct assemblyEntry *next;
    char *ucscName;
    char *description; /* Longer descriptive assembly name, from dbDb field of the same name */
    char *organism;  /* Organism name (e.g., "Human"), from dbDb field of the same name */
    char *sourceName;/* Source and date of assembly submission, from dbDb field of the same name */
    struct trackEntry *tracks;
    };


struct slName *assemblyListFromString(char *assembliesString)
/* Convert a text string with a list of assemblies into a linked list.  Assemblies can be
 * separated by any combination of spaces and punctuation */
{
if (isEmpty(assembliesString))
    return NULL;
struct slName *assemblyList = NULL;
struct kxTok *assemblyTokens = kxTokenize(assembliesString, FALSE);
struct kxTok *tok;
for (tok = assemblyTokens; tok != NULL; tok = tok->next)
    {
    /* Skip spaces and punctuation */
    if (tok->type == kxtString)
        slAddHead(&assemblyList, slNameNew(tok->string));
    }
slReverse(&assemblyList);
return assemblyList;
}


struct hash *getCustomFieldData(struct sqlConnection *host)
/* Grab custom field information for tracks from redmine and build a hash from it.
 * The hash is indexed by ticket number; values have type struct customTicketFields*. */
{
struct hash *hash = hashNew(0);
char query[4096], **row;

/* Identify indices for release log custom fields (just in case they're subject to change) */
sqlSafef (query, sizeof(query),
        "select id from custom_fields where name = \""CF_RELEASE_TEXT_NAME"\"");
int releaseTextIndex = sqlNeedQuickNum(host, query);
sqlSafef (query, sizeof(query),
        "select id from custom_fields where name = \""CF_RELEASE_URL_NAME"\"");
int releaseUrlIndex = sqlNeedQuickNum(host, query);
sqlSafef (query, sizeof(query),
        "select id from custom_fields where name = \""CF_ASSEMBLIES_NAME"\"");
int releaseAssembliesIndex = sqlNeedQuickNum(host, query);

/* Populate a hash with release-log-relevant custom fields, indexed by redmine ticket number */
sqlSafef (query, sizeof(query),
        "select customized_id, custom_field_id, value from custom_values where custom_field_id "
        " in (%d, %d, %d)", releaseTextIndex, releaseUrlIndex, releaseAssembliesIndex);
struct sqlResult *sr = sqlGetResult(host, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *ticketId = row[0];
    int customFieldId = sqlSigned(row[1]);
    char *customFieldValue = row[2];
    struct ticketCustomFields *ticketCF = hashFindVal(hash, ticketId);
    if (ticketCF == NULL)
        {
        AllocVar(ticketCF);
        ticketCF->ticketId = cloneString(ticketId);
        hashAddUnique(hash, ticketId, ticketCF);
        }
    if (customFieldId == releaseTextIndex)
        ticketCF->releaseText = cloneString(trimSpaces(customFieldValue));
    else if (customFieldId == releaseUrlIndex)
        ticketCF->releaseUrl = cloneString(trimSpaces(customFieldValue));
    else if (customFieldId == releaseAssembliesIndex)
        ticketCF->assemblies = assemblyListFromString(customFieldValue);
    else
        errAbort("Query to return release text/url/flag values instead included another custom field "
            "(field index %d != %d, %d, or %d)", customFieldId, releaseTextIndex, releaseUrlIndex, releaseAssembliesIndex);
    }
sqlFreeResult(&sr);
return hash;
}


int assemblyCompare(const void *va, const void *vb)
/* Comparison function for sorting assemblies.  First comes human, then mouse, then other organisms
 * alphabetically.  Within each organism, sort assemblies so the most recent comes first. */
{
struct assemblyEntry *as1 = *((struct assemblyEntry **)va);
struct assemblyEntry *as2 = *((struct assemblyEntry **)vb);

char *priorityAssemblies [] = {"Human", "Mouse"};
int byOrg = cmpStringOrder(as1->organism, as2->organism, priorityAssemblies, 2);
if (byOrg != 0)
    return byOrg;
else
    return -(cmpWordsWithEmbeddedNumbers(as1->ucscName, as2->ucscName));
}


struct assemblyEntry *getAllTracks(struct sqlConnection *host,
                                   struct hash *customFieldHash, char *sinceDate)
/* Build an slList of assembly entries, each of which contains an slList of tracks released on
 * that assembly.  */
{
char query[4096];
char **row;
struct assemblyEntry *theList = NULL;;
struct hash *assemblyHash = hashNew(0); /* For quickly locating a given assembly in the slList */
struct sqlConnection *central = hConnectCentral();
if (central == NULL)
    errAbort("Unable to connect to central database for dbDb information");
sqlSafef (query, sizeof(query),
        "select id from custom_fields where name = \""CF_RELEASE_FLAG_NAME"\"");
int releaseFlagIndex = sqlNeedQuickNum(host, query);
sqlSafef (query, sizeof(query),
        "select id from custom_fields where name = \""CF_RELEASE_TEXT_NAME"\"");
int releaseTextIndex = sqlNeedQuickNum(host, query);

    /* This grabs only data for tracks that are marked as having been released after sinceDate,
     * ordered by date (oldest first).  After insertion the head of each assembly's track
     * list will be the most recently released track (and the tail will be the oldest) */
sqlSafef(query, sizeof(query), "select id from issues where closed_on > '%s' and "
        "id in (select distinct customized_id from custom_values where custom_field_id = %d and "
        "value = 1) and id in (select distinct customized_id from custom_values where "
        "custom_field_id = %d and value != '') order by closed_on asc", sinceDate,
        releaseFlagIndex, releaseTextIndex);
struct sqlResult *sr = sqlGetResult(host, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *ticketId = row[0];
    struct ticketCustomFields *ticketCF = hashMustFindVal(customFieldHash, ticketId);
    struct slName *assemblyName = ticketCF->assemblies;
    if (assemblyName == NULL)
        errAbort("No assemblies listed for ticket %s, yet it has release text", ticketId);
    while(assemblyName != NULL)
        {
        struct assemblyEntry *thisAssembly = hashFindVal(assemblyHash, assemblyName->name);
        if (thisAssembly == NULL)
            {
            char centQuery[4096];
            AllocVar(thisAssembly);
            thisAssembly->ucscName = cloneString(assemblyName->name);
            sqlSafef(centQuery, sizeof(centQuery),
                    "select description, organism, sourceName from dbDb where name = \"%s\"",
                    thisAssembly->ucscName);
            struct sqlResult *centData = sqlGetResult(central, centQuery);
            char **centRow = sqlNextRow(centData);
            if (centRow == NULL)
                errAbort("Unable to find dbDb entry for assembly %s, listed in ticket %s",
                        thisAssembly->ucscName, ticketId);
            thisAssembly->description = cloneString(centRow[0]);
            thisAssembly->organism = cloneString(centRow[1]);
            thisAssembly->sourceName = cloneString(centRow[2]);
            sqlFreeResult(&centData);
            /* Add the assembly to both a local hash (for quick lookup now) and the returned list */
            hashAddUnique(assemblyHash, thisAssembly->ucscName, thisAssembly);
            slAddHead (&theList, thisAssembly);
            }

        struct trackEntry *newTrackEntry;
        AllocVar(newTrackEntry);
        newTrackEntry->ticketId = cloneString(ticketId);
        slAddHead(&(thisAssembly->tracks), newTrackEntry);
        assemblyName = assemblyName->next;
        }
    }
sqlFreeResult(&sr);
hDisconnectCentral(&central);

/* dismantle the hash without destroying the values, which are also part of the returned list */
freeHash(&assemblyHash);
slSort(&theList, &assemblyCompare);
return theList;
}


struct trackEntry *get10MostRecent(struct sqlConnection *host, char *sinceDate)
/* Return a list of the 10 most recently released tracks, ordered by descending release date */
{
struct trackEntry *theList = NULL;
char query[4096];
char **row = NULL;
sqlSafef(query, sizeof(query),
        "select id from custom_fields where name = \""CF_RELEASE_FLAG_NAME"\"");
int releaseFlagIndex = sqlNeedQuickNum(host, query);
sqlSafef (query, sizeof(query),
        "select id from custom_fields where name = \""CF_RELEASE_TEXT_NAME"\"");
int releaseTextIndex = sqlNeedQuickNum(host, query);
sqlSafef(query, sizeof(query), "select id from issues where closed_on > '%s' and id in (select "
        "distinct customized_id from custom_values where custom_field_id = %d and value = 1) and "
        "id in (select distinct customized_id from custom_values where custom_field_id = %d and "
        " value != '') order by closed_on desc limit 10", sinceDate, releaseFlagIndex,
        releaseTextIndex);
struct sqlResult *sr = sqlGetResult(host, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct trackEntry *new;
    AllocVar(new);
    new->ticketId = cloneString(row[0]);
    slAddHead(&theList, new);
    }
sqlFreeResult(&sr);
slReverse(&theList);
return theList;
}


void prettyPrintout(char *filename, struct hash *customFields, struct trackEntry *tenMostRecent,
                    struct assemblyEntry *fullList, char *oldLogFilename)
/* Write out the HTML content of a release log using the provided lists of the 10 most
 * recent tracks (can be NULL for no list), the full list of released tracks (warning generated if
 * this is NULL), and the custom field information (e.g., release log text) associated with each
 * ticket ID.  The release log is written to a file named "filename". */
{
struct htmlOutFile *output = htmlOutNewFile(filename,
        "Data Releases: Tracks, Tables, and Assemblies", "..");

struct htmlOutBlock *heading = htmlOutNewBlock(
        "<h1>Data Releases: Tracks, Tables, and Assemblies</strong></h1>", NULL);
htmlOutAddBlockContent(heading,
        "<p>\n"
        "This page contains track and table release information for the "
        "following genome assemblies:\n"
        "</p>\n");
htmlOutWriteBlock(output, heading);

struct htmlOutBlock *toc = htmlOutNewBlock(NULL, NULL);
htmlOutAddBlockContent(toc, "<p>\n");
htmlOutAddBlockContent(toc, "<div class='row'><div class='col-md-6'><ul>\n");
if (tenMostRecent != NULL)
    htmlOutAddBlockContent(toc,
            "<li><a href='#latest'><strong>10 Latest Changes (all assemblies)</strong></a></li>\n");
if (fullList == NULL)
    warn("The full list of tracks to write out is empty; something may have gone wrong");
int assemblyCount = slCount(fullList);
struct assemblyEntry *thisAssembly = fullList;
int displayedCount = 0;
while (thisAssembly != NULL)
    {
    if (displayedCount == assemblyCount/2)
        htmlOutAddBlockContent(toc, "</ul></div>\n<div class='col-md-6'><ul>\n");
    htmlOutAddBlockContent(toc, "<li><a href='#%s'>%s %s(%s)</a></li>\n", thisAssembly->ucscName,
            thisAssembly->organism, thisAssembly->description, thisAssembly->ucscName);
    thisAssembly = thisAssembly->next;
    displayedCount++;
    }
htmlOutAddBlockContent(toc, "</ul></div></div>\n</p>\n");
if (oldLogFilename != NULL)
    {
    htmlOutAddBlockContent(toc, "<p>\n"
                                "For older log entries, please see <a href=\"%s\">this page</a>.\n"
                                "</p>\n", oldLogFilename);
    }
htmlOutAddBlockContent(toc, "<p>\n");
time_t t = time(NULL);
struct tm *tmpTm = localtime(&t);
char date[1024];
strftime(date, sizeof(date), "%d %b %Y", tmpTm);
htmlOutAddBlockContent(toc, 
        "<em>Last updated %s. <a href='http://genome.ucsc.edu/contacts.html'>Inquiries and "
        "feedback welcome</a>.</em>\n</p>\n", date);
htmlOutWriteBlock(output, toc);

if (tenMostRecent != NULL)
    {
    struct htmlOutBlock *mostRecent = htmlOutNewBlock("<h2>10 Latest Changes (all assemblies)</h2>",
            "latest");
    htmlOutAddBlockContent(mostRecent, "<p>\n"
                                       "<table>\n<tbody>\n<tr>\n"
                                       "<th>Track Name</th>\n"
                                       "<th>Assembly</th>\n"
                                       "</tr>\n");
    struct trackEntry *thisTrack = tenMostRecent;
    while (thisTrack != NULL)
        {
        struct ticketCustomFields *ticketCF = hashFindVal(customFields, thisTrack->ticketId);
        char *assemblyString = slNameListToString(ticketCF->assemblies, ' ');
        if ((ticketCF->releaseUrl != NULL) && (strlen(ticketCF->releaseUrl) > 0))
            {
            htmlOutAddBlockContent(mostRecent, 
                    "<tr><td><a href='%s'>%s</a></td> <td>%s</td></tr>\n",
                    ticketCF->releaseUrl, ticketCF->releaseText, assemblyString);
            }
        else
            {
            htmlOutAddBlockContent(mostRecent, "<tr><td>%s</td> <td>%s</td></tr>\n",
                ticketCF->releaseText, assemblyString);
            }
        thisTrack = thisTrack->next;
        }
    htmlOutAddBlockContent(mostRecent, "</tbody></table>\n</p>\n");
    htmlOutWriteBlock(output, mostRecent);
    }

thisAssembly = fullList;
while (thisAssembly != NULL)
    {
    char headingText[4096];
    safef(headingText, sizeof(headingText), "<h2>%s %s (%s, %s)</h2>", thisAssembly->organism,
            thisAssembly->description, thisAssembly->ucscName, thisAssembly->sourceName);
    struct htmlOutBlock *assemblyContent = htmlOutNewBlock(headingText, thisAssembly->ucscName);
    htmlOutAddBlockContent(assemblyContent, "<p>\n"
                                            "<table>\n<tbody>\n<tr>\n"
                                            "<th>Track Name</th>\n"
                                            "</tr>\n");
    struct trackEntry *thisTrack = thisAssembly->tracks;
    while (thisTrack != NULL)
        {
        struct ticketCustomFields *ticketCF = hashFindVal(customFields, thisTrack->ticketId);
        if ((ticketCF->releaseUrl != NULL) && (strlen(ticketCF->releaseUrl) > 0))
            {
            htmlOutAddBlockContent(assemblyContent, "<tr><td><a href='%s'>%s</a></td></tr>\n",
                    ticketCF->releaseUrl, ticketCF->releaseText);
            }
        else
            {
            htmlOutAddBlockContent(assemblyContent, "<tr><td>%s</td></tr>\n",
                    ticketCF->releaseText);
            }
        thisTrack = thisTrack->next;
        }
    htmlOutAddBlockContent(assemblyContent, "</tbody></table>\n</p>\n");
    htmlOutWriteBlock(output, assemblyContent);
    htmlOutFreeBlock(&assemblyContent);
    thisAssembly = thisAssembly->next;
    }
htmlOutCloseFile(&output);
}


void uglyPrintout(struct hash *customFields, struct trackEntry *tenMostRecent,
                  struct assemblyEntry *fullList)
/* Raw text output to stdout for the full track list as well as the ten most recent tracks.  Ticket
 * IDs from redmine are included for debugging purposes */
{
printf("10 Most Recent:\n\n");
struct trackEntry *thisTrack = tenMostRecent;
while (thisTrack != NULL)
    {
    struct ticketCustomFields *ticketCF = hashFindVal(customFields, thisTrack->ticketId);
    char *assemblyString = slNameListToString(ticketCF->assemblies, ' ');
    printf ("(ID=%s) ", thisTrack->ticketId);
    if (ticketCF->releaseUrl != NULL)
        printf ("%s(%s)\t%s\n", ticketCF->releaseText, ticketCF->releaseUrl, assemblyString);
    else
        printf ("%s\t%s\n", ticketCF->releaseText, assemblyString);
    thisTrack = thisTrack->next;
    }
struct assemblyEntry *thisAssembly = fullList;
while (thisAssembly != NULL)
    {
    printf("%s %s (%s, %s)\n", thisAssembly->organism, thisAssembly->description,
            thisAssembly->ucscName, thisAssembly->sourceName);
    printf("Track Name\n");
    thisTrack = thisAssembly->tracks;
    while (thisTrack != NULL)
        {
        struct ticketCustomFields *ticketCF = hashFindVal(customFields, thisTrack->ticketId);
        printf ("(ID=%s) ", thisTrack->ticketId);
        if (ticketCF->releaseUrl != NULL)
            printf ("%s(%s)\n", ticketCF->releaseText, ticketCF->releaseUrl);
        else
            printf("%s\n", ticketCF->releaseText);
        thisTrack = thisTrack->next;
        }
    thisAssembly = thisAssembly->next;
    }
}


void buildReleaseLog(char *filename, char *sinceDate, char *oldLog)
/* buildReleaseLog - Build an updated release log based on track information pulled from a redmine
 * server.  Assumes that the redmine server credentials are stored in hg.conf under the profile
 * name "redmine", and that the relevant database on that server is also named "redmine" */
{
struct sqlConnection *redmine = sqlConnectProfile("redmine", "redmine");
struct hash *customFields = getCustomFieldData(redmine);
struct trackEntry *tenMostRecent = get10MostRecent(redmine, sinceDate);
struct assemblyEntry *fullList = getAllTracks(redmine, customFields, sinceDate);
sqlDisconnect(&redmine);

/* Spin that data into an HTML file */
prettyPrintout(filename, customFields, tenMostRecent, fullList, oldLog);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
char *sinceDate = optionVal("since", DEFAULT_SINCE_DATE);
char *oldLog = optionVal("oldLog", NULL);
buildReleaseLog(argv[1], sinceDate, oldLog);
return 0;
}
