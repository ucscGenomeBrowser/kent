/* hgBlat - CGI-script to manage fast human genome sequence searching. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "errAbort.h"
#include "hCommon.h"
#include "jksql.h"
#include "portable.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "psl.h"
#include "genoFind.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "dbDb.h"
#include "blatServers.h"
#include "web.h"
#include "hash.h"
#include "botDelay.h"
#include "trashDir.h"
#include "trackHub.h"
#include "hgConfig.h"


struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;
boolean orgChange = FALSE;
boolean dbChange = FALSE;

struct serverTable
/* Information on a server. */
   {
   char *db;		/* Database name. */
   char *genome;	/* Genome name. */
   boolean isTrans;	/* Is tranlated to protein? */
   char *host;		/* Name of machine hosting server. */
   char *port;		/* Port that hosts server. */
   char *nibDir;	/* Directory of sequence files. */
   };

char *typeList[] = {"BLAT's guess", "DNA", "protein", "translated RNA", "translated DNA"};
char *outputList[] = {"hyperlink", "psl", "psl no header"};

#ifdef LOWELAB
int minMatchShown = 14;
#else
int minMatchShown = 20;
#endif

static struct serverTable *trackHubServerTable(char *db, boolean isTrans)
/* Find out if database is a track hub with a blat server */
{
char *host, *port;

if (!trackHubGetBlatParams(db, isTrans, &host, &port))
    return NULL;

struct serverTable *st;

AllocVar(st);

st->db = cloneString(db);
st->genome = cloneString(hGenome(db));
st->isTrans = isTrans;
st->host = host; 
st->port = port;
struct trackHubGenome *genome = trackHubGetGenome(db);
st->nibDir = cloneString(genome->twoBitPath);
char *ptr = strrchr(st->nibDir, '/');
// we only want the directory name
if (ptr != NULL)
    *ptr = 0;
return st;
}

struct serverTable *findServer(char *db, boolean isTrans)
/* Return server for given database.  Db can either be
 * database name or description. */
{
if (trackHubDatabase(db))
    {
    struct serverTable *hubSt = trackHubServerTable(db, isTrans);
    if (hubSt != NULL)
	return hubSt;
    errAbort("Cannot get blat server parameters for track hub with database %s\n", db);
    }

static struct serverTable st;
struct sqlConnection *conn = hConnectCentral();
char query[256];
struct sqlResult *sr;
char **row;
char dbActualName[32];

/* If necessary convert database description to name. */
sqlSafef(query, sizeof(query), "select name from dbDb where name = '%s'", db);
if (!sqlExists(conn, query))
    {
    sqlSafef(query, sizeof(query), "select name from dbDb where description = '%s'", db);
    if (sqlQuickQuery(conn, query, dbActualName, sizeof(dbActualName)) != NULL)
        db = dbActualName;
    }

/* Do a little join to get data to fit into the serverTable. */
sqlSafef(query, sizeof(query), "select dbDb.name,dbDb.description,blatServers.isTrans"
               ",blatServers.host,blatServers.port,dbDb.nibPath "
	       "from dbDb,blatServers where blatServers.isTrans = %d and "
	       "dbDb.name = '%s' and dbDb.name = blatServers.db", 
	       isTrans, db);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    {
    errAbort("Can't find a server for %s database %s.  Click "
	     "<A HREF=\"/cgi-bin/hgBlat?%s&command=start&db=%s\">here</A> "
	     "to reset to default database.",
	     (isTrans ? "translated" : "DNA"), db,
	     cartSidUrlString(cart), hDefaultDb());
    }
st.db = cloneString(row[0]);
st.genome = cloneString(row[1]);
st.isTrans = atoi(row[2]);
st.host = cloneString(row[3]);
st.port = cloneString(row[4]);
st.nibDir = hReplaceGbdbSeqDir(row[5], st.db);

sqlFreeResult(&sr);
hDisconnectCentral(&conn);
return &st;
}

void findClosestServer(char **pDb, char **pOrg)
/* If db doesn't have a blat server, look for the closest db (or org) that has one,
 * as hgPcr does. */
{
char *db = *pDb, *org = *pOrg;

if (trackHubDatabase(db) && (trackHubServerTable(db, FALSE) != NULL))
    {
    *pDb = db;
    *pOrg = hGenome(db);
    return;
    }

struct sqlConnection *conn = hConnectCentral();
char query[256];
sqlSafef(query, sizeof(query), "select db from blatServers where db = '%s'", db);
if (!sqlExists(conn, query))
    {
    sqlSafef(query, sizeof(query), "select blatServers.db from blatServers,dbDb "
	  "where blatServers.db = dbDb.name and dbDb.genome = '%s'", org);
    char *db = sqlQuickString(conn, query);
    if (db == NULL)
	{
	sqlSafef(query, sizeof(query), "select blatServers.db from blatServers,dbDb "
	      "where blatServers.db = dbDb.name order by dbDb.orderKey,dbDb.name desc");
	char *db = sqlQuickString(conn, query);
	if (db == NULL)
	    errAbort("central database tables blatServers and dbDb are disjoint/empty");
	else
	    {
	    *pDb = db;
	    *pOrg = hGenome(db);
	    }
	}
    else
	{
	*pDb = db;
	*pOrg = hGenome(db);
	}
    }
hDisconnectCentral(&conn);
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgBlat - CGI-script to manage fast human genome sequence searching\n");
}

int countSameNonDigit(char *a, char *b)
/* Return count of characters in a,b that are the same
 * up until first digit in either one. */
{
char cA, cB;
int same = 0;
for (;;)
   {
   cA = *a++;
   cB = *b++;
   if (cA != cB)
       break;
   if (cA == 0 || cB == 0)
       break;
   if (isdigit(cA) || isdigit(cB))
       break;
   ++same;
   }
return same;
}

boolean allDigits(char *s)
/* Return TRUE if s is all digits */
{
char c;
while ((c = *s++) != 0)
    if (!isdigit(c))
        return FALSE;
return TRUE;
}



void printLuckyRedirect(char *browserUrl, struct psl *psl, char *database, char *pslName, 
			char *faName, char *uiState, char *unhideTrack)
/* Print out a very short page that redirects us. */
{
char url[1024];
safef(url, sizeof(url), "%s?position=%s:%d-%d&db=%s&ss=%s+%s&%s%s",
      browserUrl, psl->tName, psl->tStart + 1, psl->tEnd, database, 
      pslName, faName, uiState, unhideTrack);
/* htmlStart("Redirecting"); */
/* Odd it appears that we've already printed the Content-Typ:text/html line
   but I can't figure out where... */
htmStart(stdout, "Redirecting"); 
jsInlineF("location.replace('%s');\n", url);
printf("<noscript>No javascript support:<br>Click <a href='%s'>here</a> for browser.</noscript>\n", url);
htmlEnd();

}


/* forward declaration to reduce churn */
static void getCustomName(char *database, struct cart *cart, struct psl *psl, char **pName, char **pDescription);

void showAliPlaces(char *pslName, char *faName, char *customText, char *database, 
           enum gfType qType, enum gfType tType, 
           char *organism, boolean feelingLucky)
/* Show all the places that align. */
{
boolean useBigPsl = cfgOptionBooleanDefault("useBlatBigPsl", TRUE);
struct lineFile *lf = pslFileOpen(pslName);
struct psl *pslList = NULL, *psl;
char *browserUrl = hgTracksName();
char *hgcUrl = hgcName();
char uiState[64];
char *vis;
char unhideTrack[64];
char *sort = cartUsualString(cart, "sort", pslSortList[0]);
char *output = cartUsualString(cart, "output", outputList[0]);
boolean pslOut = startsWith("psl", output);
boolean isStraightNuc = (qType == gftRna || qType == gftDna);
int  minThreshold = (isStraightNuc ? minMatchShown : 0);

sprintf(uiState, "%s=%s", cartSessionVarName(), cartSessionId(cart));

/* If user has hidden BLAT track, add a setting that will unhide the 
   track if user clicks on a browser link. */
vis = cartOptionalString(cart, "hgUserPsl");
if (vis != NULL && sameString(vis, "hide"))
    snprintf(unhideTrack, sizeof(unhideTrack), "&hgUserPsl=dense");
else
    unhideTrack[0] = 0;

while ((psl = pslNext(lf)) != NULL)
    {
    if (psl->match >= minThreshold)
	slAddHead(&pslList, psl);
    }
lineFileClose(&lf);
if (pslList == NULL)
    {
    puts("<table><tr><td><hr>Sorry, no matches found<hr><td></tr></table>");
    return;
    }

pslSortListByVar(&pslList, sort);

if(feelingLucky)
    {
    /* If we found something jump browser to there. */
    if(slCount(pslList) > 0)
	printLuckyRedirect(browserUrl, pslList, database, pslName, faName, uiState, unhideTrack);
    /* Otherwise call ourselves again not feeling lucky to print empty 
       results. */
    else 
	{
	cartWebStart(cart, database, "%s BLAT Results", trackHubSkipHubName(organism));
	showAliPlaces(pslName, faName, customText, database, qType, tType, organism, FALSE);
	cartWebEnd();
	}
    }
else if (pslOut)
    {
    printf("<TT><PRE>");
    if (!sameString(output, "psl no header"))
	pslxWriteHead(stdout, qType, tType);
    for (psl = pslList; psl != NULL; psl = psl->next)
	pslTabOut(psl, stdout);
    printf("</PRE></TT>");
    }
else
    {
    printf("<H2>BLAT Search Results</H2>");
    char* posStr = cartOptionalString(cart, "position");
    if (posStr != NULL)
        printf("<P>Go back to <A HREF=\"%s\">%s</A> on the Genome Browser.</P>\n", browserUrl, posStr);

    if (useBigPsl)
        {
        char *trackName = NULL;
        char *trackDescription = NULL;
        getCustomName(database, cart, pslList, &trackName, &trackDescription);
        psl = pslList;
        printf( "<DIV STYLE=\"display:block;\"><FORM ACTION=\"%s\"  METHOD=\"POST\" NAME=\"customTrackForm\">\n", hgcUrl);
        printf("<INPUT TYPE=\"hidden\" name=\"o\" value=\"%d\" />\n",psl->tStart);
        printf("<INPUT TYPE=\"hidden\" name=\"t\" value=\"%d\" />\n",psl->tEnd);
        printf("<INPUT TYPE=\"hidden\" name=\"g\" value=\"%s\" />\n","buildBigPsl");
        printf("<INPUT TYPE=\"hidden\" name=\"i\" value=\"%s %s %s\" />\n",pslName,faName,psl->qName);
        printf("<INPUT TYPE=\"hidden\" name=\"c\" value=\"%s\" />\n",psl->tName);
        printf("<INPUT TYPE=\"hidden\" name=\"l\" value=\"%d\" />\n",psl->tStart);
        printf("<INPUT TYPE=\"hidden\" name=\"r\" value=\"%d\" />\n",psl->tEnd);
        printf("<INPUT TYPE=\"hidden\" name=\"%s\" value=\"%s\" />\n",  cartSessionVarName(), cartSessionId(cart));
        if (pslIsProtein(psl))
            printf("<INPUT TYPE=\"hidden\" name=\"isProt\" value=\"on\" />\n");

        printf("<TABLE><TR><TD>Custom track name: ");
        cgiMakeTextVar( "trackName", trackName, 30);
        printf("</TD></TR>");

        printf("<TR><TD> Custom track description: ");
        cgiMakeTextVar( "trackDescription", trackDescription,50);
        printf("</TD></TR>");
        printf("<TR><TD><INPUT TYPE=SUBMIT NAME=Submit VALUE=\"Build a custom track with these results\"></TD></TR>\n");
        printf("</TABLE></FORM></DIV>");
        }

    printf("<DIV STYLE=\"display:block;\"><PRE>");
    printf("   ACTIONS      QUERY           SCORE START  END QSIZE IDENTITY CHRO STRAND  START    END      SPAN\n");
    printf("---------------------------------------------------------------------------------------------------\n");
    for (psl = pslList; psl != NULL; psl = psl->next)
	{
        if (customText)
            printf("<A HREF=\"%s?position=%s:%d-%d&db=%s&hgt.customText=%s&%s%s\">",
                browserUrl, psl->tName, psl->tStart + 1, psl->tEnd, database, 
                customText, uiState, unhideTrack);
        else
            printf("<A HREF=\"%s?position=%s:%d-%d&db=%s&ss=%s+%s&%s%s\">",
                browserUrl, psl->tName, psl->tStart + 1, psl->tEnd, database, 
                pslName, faName, uiState, unhideTrack);
	printf("browser</A> ");
	printf("<A HREF=\"%s?o=%d&g=htcUserAli&i=%s+%s+%s&c=%s&l=%d&r=%d&db=%s&%s\">", 
	    hgcUrl, psl->tStart, pslName, cgiEncode(faName), psl->qName,  psl->tName,
	    psl->tStart, psl->tEnd, database, uiState);
	printf("details</A> ");
	printf("%-14s %5d %5d %5d %5d %5.1f%%  %4s  %2s  %9d %9d %6d\n",
	    psl->qName, pslScore(psl), psl->qStart+1, psl->qEnd, psl->qSize,
	    100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
	    skipChr(psl->tName), psl->strand, psl->tStart+1, psl->tEnd,
	    psl->tEnd - psl->tStart);
	}
    printf("</PRE>\n");
    puts("<P style=\"text-align:right\"><SMALL><A HREF=\"../FAQ/FAQblat.html#blat1b\">Missing a match?</A></SMALL></P>\n");
    puts("</DIV>\n");
    }
pslFreeList(&pslList);

}

void trimUniq(bioSeq *seqList)
/* Check that all seq's in list have a unique name.  Try and
 * abbreviate longer sequence names. */
{
struct hash *hash = newHash(0);
bioSeq *seq;

for (seq = seqList; seq != NULL; seq = seq->next)
    {
    char *saferString = needMem(strlen(seq->name)+1);
    char *c, *s;

    /*	Some chars are safe to allow through, other chars cause
     *	problems.  It isn't necessarily a URL safe string that is
     *	being calculated here.  The original problem was a user had
     *	the fasta header line of:
     *	chr8|59823648:59825047|+
     *	The plus sign was being taken as the query name and this
     *	created problems as that name was passed on to hgc via
     *	the ss cart variable.  The + sign became part of a URL
     *	eventually.  This loop allows only isalnum and =_/.:;_|
     *	to get through as part of the header name.  These characters
     *	all proved to be safe as single character names, or all
     *	together.
     */
    s = saferString;
    for (c = seq->name; *c != '\0'; ++c)
	{
	if (c && (*c != '\0'))
	    {
	    if ( isalnum(*c) || (*c == '=') || (*c == '-') || (*c == '/') ||
		(*c == '.') || (*c == ':') || (*c == ';') || (*c == '_') ||
		    (*c == '|') )
		*s++ = *c;
	    }
	}
    *s = '\0';
    freeMem(seq->name);
    if (*saferString == '\0')
	{
	freeMem(saferString);
	saferString = cloneString("YourSeq");
	}
    seq->name = saferString;

    if (strlen(seq->name) > 14)	/* Try and get rid of long NCBI .fa cruft. */
        {
	char *nameClone = NULL;
	char *abbrv = NULL;
	char *words[32];
	int wordCount;
	boolean isEns = (stringIn("ENSEMBL:", seq->name) != NULL);

	nameClone = cloneString(seq->name);
	wordCount = chopString(nameClone, "|", words, ArraySize(words));
	if (wordCount > 1)	/* Looks like it's an Ensembl/NCBI 
		                 * long name alright. */
	    {
	    if (isEns)
		{
	        abbrv = words[0];
		if (abbrv[0] == 0) abbrv = words[1];
		}
	    else if (sameString(words[1], "dbSNP"))
	        {
		if (wordCount > 2)
		    abbrv = words[2];
		else
		    abbrv = nameClone;
		}
	    else
		{
		abbrv = words[wordCount-1];
		if (abbrv[0] == 0) abbrv = words[wordCount-2];
		}
	    if (hashLookup(hash, abbrv) == NULL)
	        {
		freeMem(seq->name);
		seq->name = cloneString(abbrv);
		}
	    freez(&nameClone);
	    }
	}
    hashAddUnique(hash, seq->name, hash);
    }
freeHash(&hash);
}

int realSeqSize(bioSeq *seq, boolean isDna)
/* Return size of sequence without N's or (for proteins)
 * X's. */
{
char unknown = (isDna ? 'n' : 'X');
int i, size = seq->size, count = 0;
char *s = seq->dna;
for (i=0; i<size; ++i)
    if (s[i] != unknown) ++count;
return count;
}

void uToT(struct dnaSeq *seqList)
/* Convert any u's in sequence to t's. */
{
struct dnaSeq *seq;
for (seq = seqList; seq != NULL; seq = seq->next)
    subChar(seq->dna, 'u', 't');
}

static struct slName *namesInPsl(struct psl *psl)
/* Find all the unique names in a list of psls. */
{
struct hash *hash = newHash(3);
struct slName *nameList = NULL;
struct slName *name;
for(; psl; psl = psl->next)
    {
    if (hashLookup(hash, psl->qName) == NULL)
        {
        name = slNameNew(psl->qName);
        slAddHead(&nameList, name);
        hashStore(hash, psl->qName);
        }
    }
slReverse(&nameList);
return nameList;
}

static char *makeNameUnique(char *name, char *database, struct cart *cart)
/* Make sure track name will create a unique custom track. */
{
struct slName *browserLines = NULL;
struct customTrack *ctList = customTracksParseCart(database, cart, &browserLines, NULL);
struct customTrack *ct;
int count = 0;
char buffer[4096];
safef(buffer, sizeof buffer, "%s", name);

for(;;count++)
    {
    char *customName = customTrackTableFromLabel(buffer);
    for (ct=ctList;
         ct != NULL;
         ct=ct->next) 
        {
        if (startsWith(customName, ct->tdb->track))
            // Found a track with this name.
            break;
        }

    if (ct == NULL)
        break;

    safef(buffer, sizeof buffer, "%s (%d)", name, count + 1);
    }

return cloneString(buffer);
}

static void getCustomName(char *database, struct cart *cart, struct psl *psl, char **pName, char **pDescription)
// Find a track name that isn't currently a custom track. Also fill in description.
{
struct slName *names = namesInPsl(psl);
char shortName[4096];
char description[4096];

unsigned count = slCount(names);
if (count == 1)
    {
    safef(shortName, sizeof shortName, "blat %s", names->name);
    safef(description, sizeof description, "blat on %s",  names->name);
    }
else if (count == 2)
    {
    safef(shortName, sizeof shortName, "blat %s+%d", names->name, count - 1);
    safef(description, sizeof description, "blat on %d queries (%s, %s)", count, names->name, names->next->name);
    }
else
    {
    safef(shortName, sizeof shortName, "blat %s+%d", names->name, count - 1);
    safef(description, sizeof description, "blat on %d queries (%s, %s, ...)", count, names->name, names->next->name);
    }

*pName = makeNameUnique(shortName, database, cart);
*pDescription = cloneString(description);
}

void blatSeq(char *userSeq, char *organism, char *database)
/* Blat sequence user pasted in. */
{
FILE *f;
struct dnaSeq *seqList = NULL, *seq;
struct tempName pslTn, faTn;
int maxSingleSize, maxTotalSize, maxSeqCount;
int minSingleSize = minMatchShown;
char *genome, *db;
char *type = cgiString("type");
char *seqLetters = cloneString(userSeq);
struct serverTable *serve;
int conn;
int oneSize, totalSize = 0, seqCount = 0;
boolean isTx = FALSE;
boolean isTxTx = FALSE;
boolean txTxBoth = FALSE;
struct gfOutput *gvo;
boolean qIsProt = FALSE;
enum gfType qType, tType;
struct hash *tFileCache = gfFileCacheNew();
boolean feelingLucky = cgiBoolean("Lucky");

getDbAndGenome(cart, &db, &genome, oldVars);
if(!feelingLucky)
    cartWebStart(cart, db, "%s BLAT Results",  trackHubSkipHubName(organism));
/* Load user sequence and figure out if it is DNA or protein. */
if (sameWord(type, "DNA"))
    {
    seqList = faSeqListFromMemText(seqLetters, TRUE);
    uToT(seqList);
    isTx = FALSE;
    }
else if (sameWord(type, "translated RNA") || sameWord(type, "translated DNA"))
    {
    seqList = faSeqListFromMemText(seqLetters, TRUE);
    uToT(seqList);
    isTx = TRUE;
    isTxTx = TRUE;
    txTxBoth = sameWord(type, "translated DNA");
    }
else if (sameWord(type, "protein"))
    {
    seqList = faSeqListFromMemText(seqLetters, FALSE);
    isTx = TRUE;
    qIsProt = TRUE;
    }
else 
    {
    seqList = faSeqListFromMemTextRaw(seqLetters);
    isTx = !seqIsDna(seqList);
    if (!isTx)
	{
	for (seq = seqList; seq != NULL; seq = seq->next)
	    {
	    seq->size = dnaFilteredSize(seq->dna);
	    dnaFilter(seq->dna, seq->dna);
	    toLowerN(seq->dna, seq->size);
	    subChar(seq->dna, 'u', 't');
	    }
	}
    else
	{
	for (seq = seqList; seq != NULL; seq = seq->next)
	    {
	    seq->size = aaFilteredSize(seq->dna);
	    aaFilter(seq->dna, seq->dna);
	    toUpperN(seq->dna, seq->size);
	    }
	qIsProt = TRUE;
	}
    }
if (seqList != NULL && seqList->name[0] == 0)
    {
    freeMem(seqList->name);
    seqList->name = cloneString("YourSeq");
    }
trimUniq(seqList);

/* If feeling lucky only do the first one. */
if(feelingLucky && seqList != NULL)
    {
    seqList->next = NULL;
    }

/* Figure out size allowed. */
maxSingleSize = (isTx ? 10000 : 75000);
maxTotalSize = maxSingleSize * 2.5;
#ifdef LOWELAB
maxSeqCount = 200;
#else
maxSeqCount = 25;
#endif
char *optionMaxSeqCount = cfgOptionDefault("hgBlat.maxSequenceCount", NULL);

if (isNotEmpty(optionMaxSeqCount))
   maxSeqCount = sqlSigned(optionMaxSeqCount);

/* Create temporary file to store sequence. */
trashDirFile(&faTn, "hgSs", "hgSs", ".fa");
faWriteAll(faTn.forCgi, seqList);

/* Create a temporary .psl file with the alignments against genome. */
trashDirFile(&pslTn, "hgSs", "hgSs", ".pslx");
f = mustOpen(pslTn.forCgi, "w");
gvo = gfOutputPsl(0, qIsProt, FALSE, f, FALSE, TRUE);
serve = findServer(db, isTx);
/* Write header for extended (possibly protein) psl file. */
if (isTx)
    {
    if (isTxTx)
        {
	qType = gftDnaX;
	tType = gftDnaX;
	}
    else
        {
	qType = gftProt;
	tType = gftDnaX;
	}
    }
else
    {
    qType = gftDna;
    tType = gftDna;
    }
pslxWriteHead(f, qType, tType);

if (qType == gftProt)
    {
    minSingleSize = 14;
    }
else if (qType == gftDnaX)
    {
    minSingleSize = 36;
    }


/* Loop through each sequence. */
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    printf(" "); fflush(stdout);  /* prevent apache cgi timeout by outputting something */
    oneSize = realSeqSize(seq, !isTx);
    // Impose half the usual bot delay per sequence
	hgBotDelayFrac(0.5);
    if (++seqCount > maxSeqCount)
        {
	warn("More than %d input sequences, stopping at %s<br>(see also: cgi-bin/hg.conf hgBlat.maxSequenceCount setting).",
	    maxSeqCount, seq->name);
	break;
	}
    if (oneSize > maxSingleSize)
	{
	warn("Sequence %s is %d letters long (max is %d), skipping",
	    seq->name, oneSize, maxSingleSize);
	continue;
	}
    if (oneSize < minSingleSize)
        {
	warn("Warning: Sequence %s is only %d letters long (%d is the recommended minimum)", 
		seq->name, oneSize, minSingleSize);
	// we could use "continue;" here to actually enforce skipping, 
	// but let's give the short sequence a chance, it might work.
	// minimum possible length = tileSize+stepSize, so mpl=16 for dna stepSize=5, mpl=10 for protein.
	if (qIsProt && oneSize < 1) // protein does not tolerate oneSize==0
	    continue;
	}
    totalSize += oneSize;
    if (totalSize > maxTotalSize)
        {
	warn("Sequence %s would take us over the %d letter limit, stopping here.",
	     seq->name, maxTotalSize);
	break;
	}
    conn = gfConnect(serve->host, serve->port);
    if (isTx)
	{
	gvo->reportTargetStrand = TRUE;
	if (isTxTx)
	    {
	    gfAlignTransTrans(&conn, serve->nibDir, seq, FALSE, 5, 
	    	tFileCache, gvo, !txTxBoth);
	    if (txTxBoth)
		{
		reverseComplement(seq->dna, seq->size);
		conn = gfConnect(serve->host, serve->port);
		gfAlignTransTrans(&conn, serve->nibDir, seq, TRUE, 5, 
			tFileCache, gvo, FALSE);
		}
	    }
	else
	    {
	    gfAlignTrans(&conn, serve->nibDir, seq, 5, tFileCache, gvo);
	    }
	}
    else
	{
	gfAlignStrand(&conn, serve->nibDir, seq, FALSE, minMatchShown, tFileCache, gvo);
	reverseComplement(seq->dna, seq->size);
	conn = gfConnect(serve->host, serve->port);
	gfAlignStrand(&conn, serve->nibDir, seq, TRUE, minMatchShown, tFileCache, gvo);
	}
    gfOutputQuery(gvo, f);
    }
carefulClose(&f);

showAliPlaces(pslTn.forCgi, faTn.forCgi, NULL, serve->db, qType, tType, 
              organism, feelingLucky);
if(!feelingLucky)
    cartWebEnd();
gfFileCacheFree(&tFileCache);
}

void askForSeq(char *organism, char *db)
/* Put up a little form that asks for sequence.
 * Call self.... */
{
/* ignore struct serverTable* return, but can error out if not found */
findServer(db, FALSE);

/* JavaScript to update form when org changes */
char *onChangeText = ""
    "document.mainForm.changeInfo.value='orgChange';"
    "document.mainForm.submit();";

char *userSeq = NULL;

printf( 
"<FORM ACTION=\"../cgi-bin/hgBlat\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n"
"<H2>BLAT Search Genome</H2>\n");
cartSaveSession(cart);
puts("\n");
puts("<INPUT TYPE=HIDDEN NAME=changeInfo VALUE=\"\">\n");
puts("<TABLE BORDER=0 WIDTH=80>\n<TR>\n");
printf("<TD ALIGN=CENTER>Genome:</TD>");
printf("<TD ALIGN=CENTER>Assembly:</TD>");
printf("<TD ALIGN=CENTER>Query type:</TD>");
printf("<TD ALIGN=CENTER>Sort output:</TD>");
printf("<TD ALIGN=CENTER>Output type:</TD>");
printf("<TD ALIGN=CENTER>&nbsp</TD>");
printf("</TR>\n<TR>\n");
printf("<TD ALIGN=CENTER>\n");
printBlatGenomeListHtml(db, "change", onChangeText);
printf("</TD>\n");
printf("<TD ALIGN=CENTER>\n");
printBlatAssemblyListHtml(db);
printf("</TD>\n");
printf("<TD ALIGN=CENTER>\n");
cgiMakeDropList("type", typeList, ArraySize(typeList), NULL);
printf("</TD>\n");
printf("<TD ALIGN=CENTER>\n");
cgiMakeDropList("sort", pslSortList, ArraySize(pslSortList), cartOptionalString(cart, "sort"));
printf("</TD>\n");
printf("<TD ALIGN=CENTER>\n");
cgiMakeDropList("output", outputList, ArraySize(outputList), cartOptionalString(cart, "output"));
printf("</TD>\n");
printf("</TR>\n<TR>\n");
userSeq = cartUsualString(cart, "userSeq", "");
printf("<TD COLSPAN=5 ALIGN=CENTER>\n");
htmlPrintf("<TEXTAREA NAME=userSeq ROWS=14 COLS=80>%s</TEXTAREA>\n", userSeq);
printf("</TD>\n");
printf("</TR>\n<TR>\n");
printf("<TD COLSPAN=5 ALIGN=CENTER>\n");
printf("<INPUT TYPE=SUBMIT NAME=Submit VALUE=submit>\n");
printf("<INPUT TYPE=SUBMIT NAME=Lucky VALUE=\"I'm feeling lucky\">\n");
printf("<INPUT TYPE=SUBMIT NAME=Clear VALUE=clear>\n");
printf("</TD>\n");
printf("</TR>\n<TR>\n"); 
puts("<TD COLSPAN=5 WIDTH=\"100%\">\n" 
    "Paste in a query sequence to find its location in the\n"
    "the genome. Multiple sequences may be searched \n"
    "if separated by lines starting with '>' followed by the sequence name.\n"
    "</TD>\n"
    "</TR>\n"

);
puts("<TR><TD COLSPAN=5 WIDTH=\"100%\">\n"); 
puts("<BR><B>File Upload:</B> ");
puts("Rather than pasting a sequence, you can choose to upload a text file containing "
	 "the sequence.<BR>");
puts("Upload sequence: <INPUT TYPE=FILE NAME=\"seqFile\">");
puts(" <INPUT TYPE=SUBMIT Name=Submit VALUE=\"submit file\"><P>\n");
printf("%s", 
"<P>Only DNA sequences of 25,000 or fewer bases and protein or translated \n"
"sequence of 10000 or fewer letters will be processed.  Up to 25 sequences\n"
"can be submitted at the same time. The total limit for multiple sequence\n"
"submissions is 50,000 bases or 25,000 letters.\n</P>");
if (hgPcrOk(db))
    printf("<P>For locating PCR primers, use <A HREF=\"../cgi-bin/hgPcr?db=%s\">In-Silico PCR</A>"
           " for best results instead of BLAT.</P>", db);
puts("</TD></TR></TABLE>\n");



printf("</FORM>\n");

webNewSection("About BLAT");
printf( 
"<P>BLAT on DNA is designed to\n"
"quickly find sequences of 95%% and greater similarity of length 25 bases or\n"
"more.  It may miss more divergent or shorter sequence alignments.  It will find\n"
"perfect sequence matches of 20 bases.\n"
"BLAT on proteins finds sequences of 80%% and greater similarity of length 20 amino\n"
"acids or more.  In practice DNA BLAT works well on primates, and protein\n"
"blat on land vertebrates."
);


printf("%s",
"\n</P><P>BLAT is not BLAST.  DNA BLAT works by keeping an index of the entire genome\n"
"in memory.  The index consists of all overlapping 11-mers stepping by 5 except for\n"
"those heavily involved in repeats.  The index takes up about\n"
"2 gigabytes of RAM.  RAM can be further reduced to less than 1 GB by increasing step size to 11.\n"
"The genome itself is not kept in memory, allowing\n"
"BLAT to deliver high performance on a reasonably priced Linux box.\n"
"The index is used to find areas of probable homology, which are then\n"
"loaded into memory for a detailed alignment. Protein BLAT works in a similar\n"
"manner, except with 4-mers rather than 11-mers.  The protein index takes a little\n"
"more than 2 gigabytes.</P>\n"
"<P>BLAT was written by <A HREF=\"mailto:kent@soe.ucsc.edu\">Jim Kent</A>.\n"
"Like most of Jim's software, interactive use on this web server is free to all.\n"
"Sources and executables to run batch jobs on your own server are available free\n"
"for academic, personal, and non-profit purposes.  Non-exclusive commercial\n"
"licenses are also available. See the \n"
"<A HREF=\"http://www.kentinformatics.com\" TARGET=_blank>Kent Informatics</A>\n"
"website for details.</P>\n"
"\n"
"<P>For more information on the graphical version of BLAT, click the Help \n"
"button on the top menu bar");

if (hIsGsidServer())
    printf(". </P> \n");
else
    printf(" or see the Genome Browser <A HREF=\"../FAQ/FAQblat.html\">FAQ</A>. </P> \n");

}

void doMiddle(struct cart *theCart)
/* Write header and body of html page. */
{
char *userSeq;
char *db, *organism;
boolean clearUserSeq = cgiBoolean("Clear");

cart = theCart;
dnaUtilOpen();

orgChange = sameOk(cgiOptionalString("changeInfo"),"orgChange");
if (orgChange)
    {
    cgiVarSet("db", hDefaultDbForGenome(cgiOptionalString("org"))); 
    }
getDbAndGenome(cart, &db, &organism, oldVars);
char *oldDb = cloneString(db);
findClosestServer(&db, &organism);

/* Get sequence - from userSeq variable, or if 
 * that is empty from a file. */
if (clearUserSeq)
    {
    cartSetString(cart, "userSeq", "");
    cartSetString(cart, "seqFile", "");
    }
userSeq = cartUsualString(cart, "userSeq", "");
if (isEmpty(userSeq))
    {
    userSeq = cartOptionalString(cart, "seqFile");
    }
if (isEmpty(userSeq) || orgChange)
    {
    cartWebStart(theCart, db, "%s BLAT Search", trackHubSkipHubName(organism));
    if (differentString(oldDb, db))
	printf("<HR><P><EM><B>Note:</B> BLAT search is not available for %s %s; "
	       "defaulting to %s %s</EM></P><HR>\n",
	       hGenome(oldDb), hFreezeDate(oldDb), organism, hFreezeDate(db));
    askForSeq(organism,db);
    cartWebEnd();
    }
else 
    {
    blatSeq(skipLeadingSpaces(userSeq), organism, db);
    }
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "Clear", "Lucky", "type", "userSeq", "seqFile", "showPage", "changeInfo", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
oldVars = hashNew(10);
cgiSpoof(&argc, argv);

/* org has precedence over db when changeInfo='orgChange' */

cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgBlat", enteredMainTime);
return 0;
}

