/* pubs.c - display details of publiations literature track (pubsxxx tables) */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgc.h"
#include "hgColors.h"
#include "trackDb.h"
#include "web.h"
#include "hash.h"
#include "obscure.h"
#include "common.h"
#include "string.h"
//include "hgTrackUi.h"

// cgi var to activate debug output
static int pubsDebug = 0;

// internal section types in mysql table
static char* pubsSecNames[] ={
      "header", "abstract",
      "intro", "methods",
      "results", "discussion",
      "conclusions", "ack",
      "refs", "unknown" };
//
// whether a checkbox is checked by default, have to correspond to pubsSecNames
static int pubsSecChecked[] ={
      1, 1,
      1, 1,
      1, 1,
      1, 0,
      0, 1 };

static char* pubsSequenceTable;

static char* mangleUrl(char* url) 
/* add publisher specific parameters to url and return new url*/
{
if (!stringIn("sciencedirect.com", url))
    return url;
    
// cgi param to add the "UCSC matches" sciverse application to elsevier's sciencedirect
char* sdAddParam = "?svAppaddApp=298535"; 
char* longUrl = catTwoStrings(url, sdAddParam);
char* newUrl = replaceChars(longUrl, "article", "svapps");
return newUrl;
}

static void printFilterLink(char* pslTrack, char* articleId)
/* print a link to hgTracks with an additional cgi param to activate the single article filter */
{
    int start = cgiInt("o");
    int end = cgiInt("t");
    printf("&nbsp; <A HREF=\"%s&amp;db=%s&amp;position=%s%%3A%d-%d&amp;pubsFilterArticleId=%s&amp;%s=pack\">",
                      hgTracksPathAndSettings(), database, seqName, start+1, end, articleId, pslTrack);
    char startBuf[64], endBuf[64];
    sprintLongWithCommas(startBuf, start + 1);
    sprintLongWithCommas(endBuf, end);
    printf("Show these sequence matches individually on genome browser</A>");
}

static char* makeSqlMarkerList(void)
/* return list of sections from cgi vars, format like "'abstract','header'" */
{
int secCount = sizeof(pubsSecNames)/sizeof(char *);
struct slName* names = NULL;
int i;
for (i=0; i<secCount; i++) 
{
    // add ' around name and add to list
    char* secName = pubsSecNames[i];
    if (cgiOptionalInt(secName, pubsSecChecked[i]))
    {
        char nameBuf[100];
        safef(nameBuf, sizeof(nameBuf), "'%s'", secName);
        slAddHead(&names, slNameNew(nameBuf));
    }
}

if (names==0)
    errAbort("You need to specify at least one article section.");

char* nameListString = slNameListToString(names, ',');
slNameFree(names);
return nameListString;
}


static struct sqlResult* queryMarkerRows(struct sqlConnection* conn, char* markerTable, \
    char* articleTable, char* item, int itemLimit, char* sectionList)
/* query marker rows from mysql, based on http parameters  */
{
char query[4000];
/* Mysql specific setting to make the group_concat function return longer strings */
sqlUpdate(conn, "SET SESSION group_concat_max_len = 100000");

safef(query, sizeof(query), "SELECT distinct %s.articleId, url, title, authors, citation, pmid, "  
    "group_concat(snippet, concat(\" (section: \", section, \")\") SEPARATOR ' (...) ') FROM %s "
    "JOIN %s USING (articleId) "
    "WHERE markerId='%s' AND section in (%s) "
    "GROUP by articleId "
    "ORDER BY year DESC "
    "LIMIT %d",
    markerTable, markerTable, articleTable, item, sectionList, itemLimit);

if (pubsDebug)
    printf("%s", query);

struct sqlResult *sr = sqlGetResult(conn, query);

return sr;
}


static void printSectionCheckboxes()
/* show a little form with checkboxes where user can select sections they want to show */
{
// labels to show to user, have to correspond to pubsSecNames
char *secLabels[] ={
      "Title", "Abstract",
      "Introduction", "Methods",
      "Results", "Discussion",
      "Conclusions", "Acknowledgements",
      "References", "Not determined" };

int labelCount = sizeof(secLabels)/sizeof(char *);

int i;
printf("<P>\n");
printf("<B>Sections of article shown:</B><BR>\n");
printf("<FORM ACTION=\"hgc?%s&o=%s&t=%s&g=%s&i=%s\" METHOD=\"get\">\n",
    cartSidUrlString(cart), cgiString("o"), cgiString("t"), cgiString("g"), cgiString("i"));

for (i=0; i<labelCount; i++) 
{
    char* name = pubsSecNames[i];
    // checkboxes default to 0 unless checked, see 
    // http://stackoverflow.com/questions/2520952/how-come-checkbox-state-is-not-always-passed-along-to-php-script
    printf("<INPUT TYPE=\"hidden\" name=\"%s\" value=\"0\" />\n", pubsSecNames[i]);
    printf("<INPUT TYPE=\"checkbox\" name=\"%s\" ", name);

    int isChecked = cgiOptionalInt(name, pubsSecChecked[i]);
    if (isChecked)
        printf("value=\"1\" checked=\"yes\">%s</INPUT>\n", secLabels[i]);
    else
        printf("value=\"1\">%s</INPUT>\n", secLabels[i]);
}

printf("<INPUT TYPE=\"hidden\" name=\"o\" value=\"%s\" />\n", cgiString("o"));
printf("<INPUT TYPE=\"hidden\" name=\"g\" value=\"%s\" />\n", cgiString("g"));
printf("<INPUT TYPE=\"hidden\" name=\"t\" value=\"%s\" />\n", cgiString("t"));
printf("<INPUT TYPE=\"hidden\" name=\"i\" value=\"%s\" />\n", cgiString("i"));
printf("<INPUT TYPE=\"hidden\" name=\"hgsid\" value=\"%d\" />\n", cart->sessionId);
printf("<BR>");
printf("<INPUT TYPE=\"submit\" VALUE=\"Submit\" />\n");
printf("</FORM><P>\n");
}

static void printLimitWarning(struct sqlConnection *conn, char* markerTable, 
    char* item, int itemLimit, char* sectionList)
{
char query[4000];
safef(query, sizeof(query), "SELECT COUNT(*) from %s WHERE markerId='%s' AND section in (%s) ", markerTable, item, sectionList);
if (sqlNeedQuickNum(conn, query) > itemLimit) 
{
    printf("<b>This marker is mentioned more than %d times</b><BR>\n", itemLimit);
    printf("The results would take too long to load in your browser and are "
    "therefore limited to %d articles.<P>\n", itemLimit);
}
}

static void printMarkerSnippets(struct sqlConnection *conn, char* articleTable, char* markerTable, char* item)
{

/* do not show more snippets than this limit */
int itemLimit=1000;

printSectionCheckboxes();
char* sectionList = makeSqlMarkerList();
printLimitWarning(conn, markerTable, item, itemLimit, sectionList);

printf("<H3>Snippets from Publications:</H3>");
struct sqlResult* sr = queryMarkerRows(conn, markerTable, articleTable, item, itemLimit, sectionList);

char **row;
while ((row = sqlNextRow(sr)) != NULL)
{
    char* articleId = row[0];
    char* url       = row[1];
    char* title     = row[2];
    char* authors   = row[3];
    char* citation  = row[4];
    char* pmid      = row[5];
    char* snippets  = row[6];
    url = mangleUrl(url);
    printf("<A HREF=\"%s\">%s</A> ", url, title);
    printf("<SMALL>%s</SMALL>; ", authors);
    printf("<SMALL>%s ", citation);
    if (!isEmpty(pmid) && strcmp(pmid, "0")!=0 )
        printf(", <A HREF=\"http://www.ncbi.nlm.nih.gov/pubmed/%s\">PMID%s</A>\n", pmid, pmid);
    printf("</SMALL><BR>\n");
    if (pubsDebug)
        printf("articleId=%s", articleId);
    printf("<I>%s</I><P>", snippets);
    printf("<HR>");
}

freeMem(sectionList);
sqlFreeResult(&sr);
}

static char* printArticleInfo(struct sqlConnection *conn, char* item, char* pubsArticleTable)
/* Header with information about paper, return documentId */
{
    char query[512];

    safef(query, sizeof(query), "SELECT articleId, url, title, authors, citation, abstract, pmid FROM %s WHERE articleId='%s'", pubsArticleTable, item);

    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    char *articleId=NULL;
    if ((row = sqlNextRow(sr)) == NULL)
    {
        printf("Could not resolve articleId %s, this is an internal error.\n", item);
        printf("Please send an email to max@soe.ucsc.edu\n");
        sqlFreeResult(&sr);
        return NULL;
    }

    articleId = cloneString(row[0]);
    char* url      = row[1];
    char* title    = row[2];
    char* authors  = row[3];
    char* cit      = row[4];
    char* abstract = row[5];
    char* pmid     = row[6];

    url = mangleUrl(url);
    if (strlen(abstract)==0) 
            abstract = "(No abstract available for this article. "
                "Please follow the link to the fulltext above.)";

    printf("<P>%s</P>\n", authors);
    printf("<A TARGET=\"_blank\" HREF=\"%s\"><B>%s</B></A>\n", url, title);
    printf("<P style=\"width:800px; font-size:80%%\">%s", cit);
    if (strlen(pmid)!=0 && strcmp(pmid, "0"))
        printf(", <A HREF=\"http://www.ncbi.nlm.nih.gov/pubmed/%s\">PMID%s</A>\n", pmid, pmid);
    printf("</P>\n");
    printf("<P style=\"width:800px; font-size:100%%\">%s</P>\n", abstract);

    sqlFreeResult(&sr);
    return articleId;
}

static struct hash* getSeqIdHash(struct sqlConnection* conn, char* trackTable, \
    char* articleId, char *item, char* seqName, int start)
/* return a hash with the sequence IDs for a given chain of BLAT matches */
{
    char query[512];
    /* check first if the column exists (some debugging tables on hgwdev don't have seqIds) */
    safef(query, sizeof(query), "SHOW COLUMNS FROM %s LIKE 'seqIds';", trackTable);
    char* seqIdPresent = sqlQuickString(conn, query);
    if (!seqIdPresent) {
        return NULL;
    }

    /* get sequence-Ids for feature that was clicked (item&startPos are unique) and return as hash*/
    safef(query, sizeof(query), "SELECT seqIds,'' FROM %s WHERE name='%s' "
        "and chrom='%s' and chromStart=%d;", trackTable, item, seqName, start);
    if (pubsDebug)
        printf("%s<br>", query);
    
    // split comma-sep list into parts
    char* seqIdCoordString = sqlQuickString(conn, query);
    char* seqIdCoords[1024];
    int partCount = chopString(seqIdCoordString, ",", seqIdCoords, ArraySize(seqIdCoords));
    int i;

    struct hash *seqIdHash = NULL;
    seqIdHash = newHash(0);
    for (i=0; i<partCount; i++) 
    {
        hashAdd(seqIdHash, seqIdCoords[i], NULL);
    }
    return seqIdHash;
}

static void printSeqHeaders(bool showDesc, bool isClickedSection) 
{
    printf("<TABLE style=\"background-color: #%s\" WIDTH=\"100%%\" CELLPADDING=\"2\">\n", HG_COL_BORDER);
    printf("<TR style=\"background-color: #%s; color: #FFFFFF\">\n", HG_COL_TABLE_LABEL);
    if (showDesc)
        puts("  <TH style=\"width: 10%\">Article file</TH>\n");
    puts("  <TH style=\"width: 60%\">One row per sequence, with flanking text, sequence in bold</TH>\n");
    if (pubsDebug)
        puts("  <TH style=\"width: 30%\">Identifiers</TH>\n");

    if (!isClickedSection && !pubsDebug)
        puts("  <TH style=\"width: 20%\">Chained matches with this sequence</TH>\n");
    puts("</TR>\n");
}

static void printAddWbr(char* text, int distance) 
/* a crazy hack for firefox/mozilla that is unable to break long words in tables
 * We need to add a <wbr> tag every x characters in the text to make text breakable.
 */
{
int i;
i = 0;
char* c;
c = text;
bool doNotBreak = FALSE;
while (*c != 0) {
    {
    if ((*c=='&') || (*c=='<'))
       doNotBreak = TRUE;
    if (*c==';' || (*c =='>'))
       doNotBreak = FALSE;

    printf("%c", *c);
    if (i % distance == 0 && ! doNotBreak) 
        printf("<wbr>");
    c++;
    i++;
    }
}
}

void printHgTracksLink(char* db, char* chrom, int start, int end, char* linkText, char* optUrlStr)
/* print link to hgTracks for db at pos */
{
char buf[1024];
if (linkText==NULL) 
{
    char startBuf[64], endBuf[64];
    sprintLongWithCommas(startBuf, start + 1);
    sprintLongWithCommas(endBuf, end);
    safef(buf, sizeof(buf), "%s:%s-%s (%s)", seqName, startBuf, endBuf, db);
    linkText = buf;
}

if (optUrlStr==NULL)
    optUrlStr = "";
    
printf("<A HREF=\"%s&amp;db=%s&amp;position=%d-%d&amp;%s\">%s</A>\n", hgTracksPathAndSettings(), db, start, end, optUrlStr, linkText);
}

void printGbLinks(struct slName* locs) 
/* print hash keys in format hg19/chr1:1-1000 as links */
{
struct slName *el;
for (el = locs; el != NULL; el = el->next) 
{
    char* locString = el->name;
    char* db       = cloneNextWordByDelimiter(&locString, '/');
    char* chrom    = cloneNextWordByDelimiter(&locString, ':');
    char* startStr = cloneNextWordByDelimiter(&locString, '-');
    char* endStr   = locString;

    int start = atoi(startStr);
    int end = atoi(endStr);
    printHgTracksLink(db, chrom, start, end, NULL, NULL);
    printf("<BR>");
    //freeMem(endStr); //XX why can't I free these?
    //freeMem(chrom);
    //freeMem(startStr);
    //freeMem(db);
}
}

static bool printSeqSection(char* articleId, char* title, bool showDesc, struct sqlConnection* conn, struct hash* clickedSeqs, bool isClickedSection, bool fasta, char* pslTable)
/* print a table of sequences, show only sequences with IDs in hash,
 * There are two sections, respective sequences are shown depending on isClickedSection and clickedSeqs 
 *   - seqs that were clicked on (isClickedSection=True) -> show only seqs in clickedSeqs
 *   - other seqs (isClickedSection=False) -> show all other seqs
 * 
 * */
{
    // get data from mysql
    char query[4096];
    safef(query, sizeof(query), 
    "SELECT fileDesc, snippet, locations, articleId, fileId, seqId, sequence "
    "FROM %s WHERE articleId='%s';", pubsSequenceTable, articleId);
    if (pubsDebug)
        puts(query);
    struct sqlResult *sr = sqlGetResult(conn, query);

    // construct title for section
    char* otherFormat = NULL;
    if (fasta)
        otherFormat = "table";
    else
        otherFormat = "fasta";

    char fullTitle[5000];
    safef(fullTitle, sizeof(fullTitle), 
    "%s&nbsp;<A HREF=\"../cgi-bin/hgc?%s&o=%s&t=%s&g=%s&i=%s&fasta=%d\"><SMALL>(%s format)</SMALL></A>", 
    title, cartSidUrlString(cart), cgiString("o"), cgiString("t"), cgiString("g"), cgiString("i"), 
    !fasta, otherFormat);

    webNewSection(fullTitle);

    if (!fasta) 
        printSeqHeaders(showDesc, isClickedSection);

    char **row;
    bool foundSkippedRows = FALSE;
    while ((row = sqlNextRow(sr)) != NULL)
    {
        char* fileDesc = row[0];
        char* snippet  = row[1];
        char* locString= row[2];
        char* artId    = row[3];
        char* fileId   = row[4];
        char* seqId    = row[5];
        char* seq      = row[6];

        // annotation (=sequence) ID is a 64 bit int with 10 digits for 
        // article, 3 digits for file, 5 for annotation
        char annotId[100];
        safef(annotId, 100, "%010d%03d%05d", atoi(artId), atoi(fileId), atoi(seqId));
        if (pubsDebug)
            printf("%s", annotId);

        // only display this sequence if we're in the right section
        if (clickedSeqs!=NULL && ((hashLookup(clickedSeqs, annotId)!=NULL) != isClickedSection)) {
            foundSkippedRows = TRUE;
            continue;
        }

        if (fasta)
        {
            printf("<TR><TD><TT>>%s<BR>%s<BR></TT></TD></TR></TABLE>", annotId, seq);
        }
        else
        {
            printf("<TR style=\"background-color: #%s\">\n", HG_COL_LOCAL_TABLE);
            if (showDesc)
                printf("<TD style=\"word-break:break-all\">%s\n", fileDesc);
            //printf("<TD><I>%s</I></TD>\n", snippet); 
            printf("<TD style=\"word-break:break-all;\"><I>");
            printAddWbr(snippet, 40);
            printf("</I></TD>\n"); 
            if (pubsDebug) 
            {
                printf("<TD>article %s, file %s, seq %s, annotId %s", artId, fileId, seqId, annotId);
            }

            // print links to locations 
            if (!isClickedSection && !pubsDebug) {
                // format: hg19/chr1:300-400,mm9/chr1:60006-23234
                // split on "," then split on "/"
                //locs = charSepToSlNames(locString, ',');

                char* locArr[1024];
                int partCount = chopString(locString, ",", locArr, ArraySize(locArr));
                printf("<TD>");
                if (partCount==0)
                    printf("No matches");

                else
                {
                struct slName *locs;
                locs = slNameListFromStringArray(locArr, partCount);
                slUniqify(&locs, slNameCmp, slNameFree);
                printGbLinks(locs);
                printf("<BR>");
                printf("</TD>\n");
                slFreeList(&locs);
                }

            }
        printf("</TR>\n");
        }
	}
    printf("</TR>\n");

    if (isClickedSection)
    {
        printf("</TABLE></TABLE><TR><TD><P>&nbsp;");
        printFilterLink(pslTable, articleId);
    }
    webEndSectionTables();
    sqlFreeResult(&sr);
    return foundSkippedRows;
}

static void printSeqInfo(struct sqlConnection* conn, char* trackTable,
    char* pslTable, char* articleId, char* item, char* seqName, int start, 
    bool fileDesc, bool fasta)
    /* print sequences, split into two sections 
     * two sections: one for sequences that were clicked, one for all others*/
{
    struct hash* clickedSeqs = getSeqIdHash(conn, trackTable, articleId, item, seqName, start);

    bool skippedRows;
    if (clickedSeqs) 
        skippedRows = printSeqSection(articleId, "Sequences used to construct this feature", \
            fileDesc, conn, clickedSeqs, 1, fasta, pslTable);
    else 
        skippedRows=1;

    if (skippedRows)
        printSeqSection(articleId, "Other Sequences in this article", \
            fileDesc, conn, clickedSeqs, 0, fasta, pslTable);
    printf("<P><SMALL>Copyright 2012 Elsevier B.V. All rights reserved.</SMALL><P>");
    freeHash(&clickedSeqs);

}

static void printTrackVersion(struct trackDb *tdb, struct sqlConnection* conn, char* item) 
{
    char versionString[256];
    char dateReference[256];
    char headerTitle[512];
    /* see if hgFixed.trackVersion exists */
    boolean trackVersionExists = hTableExists("hgFixed", "trackVersion");

    if (trackVersionExists)
        {
        char query[256];
        safef(query, sizeof(query), \
        "SELECT version,dateReference FROM hgFixed.trackVersion "
        "WHERE db = '%s' AND name = 'pubs' ORDER BY updateTime DESC limit 1", database);
        struct sqlResult *sr = sqlGetResult(conn, query);
        char **row;

        /* in case of NULL result from the table */
        versionString[0] = 0;
        while ((row = sqlNextRow(sr)) != NULL)
        {
        safef(versionString, sizeof(versionString), "version %s",
            row[0]);
        safef(dateReference, sizeof(dateReference), "%s",
            row[1]);
        }
        sqlFreeResult(&sr);
        }
    else
        {
        versionString[0] = 0;
        dateReference[0] = 0;
        }

    if (versionString[0])
        safef(headerTitle, sizeof(headerTitle), "%s - %s", item, versionString);
    else
        safef(headerTitle, sizeof(headerTitle), "%s", item);

    genericHeader(tdb, headerTitle);
}

static void printPositionAndSize(int start, int end, bool showSize)
{
    printf("<B>Position:</B>&nbsp;"
               "<A HREF=\"%s&amp;db=%s&amp;position=%s%%3A%d-%d\">",
                      hgTracksPathAndSettings(), database, seqName, start+1, end);
    char startBuf[64], endBuf[64];
    sprintLongWithCommas(startBuf, start + 1);
    sprintLongWithCommas(endBuf, end);
    printf("%s:%s-%s</A><BR>\n", seqName, startBuf, endBuf);
    long size = end - start;
    sprintLongWithCommas(startBuf, size);
    if (showSize)
        printf("<B>Genomic Size:</B>&nbsp;%s<BR>\n", startBuf);
}

static bioSeq *getSeq(struct sqlConnection *conn, char *table, char *id)
/* copied from otherOrgs.c */
{
char query[512];
struct sqlResult *sr;
char **row;
bioSeq *seq = NULL;
safef(query, sizeof(query), 
    "select sequence from %s where annotId = '%s'", table, id);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(seq);
    seq->name = cloneString(id);
    seq->dna = cloneString(row[0]);
    seq->size = strlen(seq->dna);
    }
sqlFreeResult(&sr);

return seq;
}

void pubsAli(struct sqlConnection *conn, char *pslTable, char *seqTable, char *item)
/* this is just a ripoff from htcCdnaAli, similar to markd's transMapAli */
{
bioSeq *oSeq = NULL;
writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>Literature Sequence vs Genomic</TITLE>\n</HEAD>\n\n");

struct psl *psl = getAlignments(conn, pslTable, item);
if (psl == NULL)
    errAbort("Couldn't find alignment at %s:%s", pslTable, item);

oSeq = getSeq(conn, seqTable, item);
if (oSeq->size != psl->qSize) 
{
    //errAbort("prot alignments not supported yet");
    //oSeq -> size = 3*oSeq -> size;
    errAbort("prot alignments not supported yet %s", oSeq->dna);
    oSeq = translateSeq(oSeq, 0, FALSE);
    errAbort("prot alignments not supported yet %s", oSeq->dna);
}
if (oSeq == NULL)  
    errAbort("%s is in pslTable but not in sequence table. Internal error.", item);
showSomeAlignment(psl, oSeq, gftDna, 0, oSeq->size, NULL, 0, 0);
printf("hihi");
}

void doPubsDetails(struct trackDb *tdb, char *item)
/* publications custom display */
{

int start        = cgiInt("o");
int end          = cgiOptionalInt("t", 0);
char* trackTable = cgiString("g");
char* aliTable   = cgiOptionalString("aliTable");
int fasta        = cgiOptionalInt("fasta", 0);
pubsDebug        = cgiOptionalInt("debug", 0);

struct sqlConnection *conn = hAllocConn(database);

char* articleTable = trackDbRequiredSetting(tdb, "pubsArticleTable");

if (stringIn("Psl", trackTable))
{ 
    if (aliTable!=NULL)
    {
        pubsSequenceTable = trackDbRequiredSetting(tdb, "pubsSequenceTable");
        pubsAli(conn, trackTable, pubsSequenceTable, item);
        return;
    }

    else
    {
        genericHeader(tdb, item);
        struct psl *psl = getAlignments(conn, trackTable, item);
        printf("<H3>Genomic Alignment with sequence found in publication fulltext</H3>");
        printAlignmentsSimple(psl, start, trackTable, trackTable, item);
    }
}

else
{
    printTrackVersion(tdb, conn, item);
    if (stringIn("Marker", trackTable))
    {
        char* markerTable = trackDbRequiredSetting(tdb, "pubsMarkerTable");
        printPositionAndSize(start, end, 0);
        printMarkerSnippets(conn, articleTable, markerTable, item);
    }
    else
    {
        printPositionAndSize(start, end, 1);
        pubsSequenceTable = trackDbRequiredSetting(tdb, "pubsSequenceTable");
        char* articleId = printArticleInfo(conn, item, articleTable);
        if (articleId!=NULL) 
        {
            bool showDesc; 
            showDesc = (! endsWith(trackTable, "Elsevier")); 
            // avoid clutter: Elsevier has only main text
            char *pslTable = trackDbRequiredSetting(tdb, "pubsPslTrack");
            printSeqInfo(conn, trackTable, pslTable, articleId, item, seqName, start, showDesc, fasta);
        }
    }
}

printTrackHtml(tdb);
hFreeConn(&conn);
}
