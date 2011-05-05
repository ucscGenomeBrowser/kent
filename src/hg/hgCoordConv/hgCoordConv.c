/**
   cgi program to convert coordinates from one draft of the
   genome to another. Cool place to test is: chr1:126168504-126599722
   from Dec to April, it was inverted. Looks to be inverted back in
   Aug.

   Note: This now should do conversion between zoo species based on tracks in each zoo database.
   Note: Troubleshooting features have not been fully implemented for Zoo conversions.
*/
#include "common.h"
#include "obscure.h"
#include "errabort.h"
#include "htmshell.h"
#include "jksql.h"
#include "coordConv.h"
#include "hdb.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "web.h"
#include "cheapcgi.h"
#include "psl.h"
#include "dystring.h"
#include "dbDb.h"
#include "blatServers.h"
#include "cart.h"
#include "hdb.h"
#include "hui.h"
#include "botDelay.h"

static char const rcsid[] = "$Id: hgCoordConv.c,v 1.27 2008/09/03 19:18:48 markd Exp $";

/* these variables are used for testing mode */
boolean hgTest = FALSE;          /* are we in testing mode ? */
int numTests = 0;               /* how many tests should we do */
int hgTestCorrect = 0;          /* how many we called correct and were placed correctly */
int hgTestWrong = 0;            /* how many we called correct and were placed incorrectly */

/* these variables are used for links and if the browser is
   called by cgi */
char *chrom = NULL;            /* which chromosom are we on */
int chromStart = -1;           /* chromStart on chrom */
int chromEnd = -1;             /* chromEnd on chrom */

bool calledSelf = FALSE;     /* decide whether to do form or convert coordinates */
char *browserUrl = "../cgi-bin/hgTracks?";  /* link to browser */
char *blatUrl = "../cgi-bin/hgBlat?";       /* link to blat */

char *position = NULL;        /* position string in form of chrN:1-100000 */
char *newGenome = NULL;       /* name of genome converting to */
char *origGenome = NULL;      /* name of genome converting from */
char *defaultPos = "chr22:17045228-17054909"; /* default position */
char *origDb = NULL;          /* name of genome to use as from genome, optional */
char *organism = "Human";     /* Only do human for now. */
struct dyString *webWarning = NULL; /* set this string with warnings for user,
				       displayed on results page */

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "type", "genome", "userSeq", "seqFile", NULL};
struct cart *cart;


void usage()
{
errAbort("hgCoordConv - cgi program for converting coordinates from one draft\n"
	 "of the human genome to another. Can be tested from command line by trying.\n"
	 "to align random bits of genome back to same draft.\n"
	 "usage:\n\thgCoordConv hgTest=on numTests=10 origGenome=hg10 newGenome=hg10\n");
}


/* Zoo count  is very similar to its human brethren below except it searchers for zoo*/
int zooCount(struct dbDb *dbList)
/* Count the number of human organism records. */
{
struct dbDb *db = NULL;
int count = 0;
for(db = dbList; db != NULL; db = db->next)
    {

    /*  Make sure zoo Combo isn't included and that the species are zoo */
    if(strstrNoCase(db->name, "zoo") && !strstrNoCase(db->name,"combo"))
	count++;
    }
return count;
}

int humanCount(struct dbDb *dbList)
/* Count the number of human organism records. */
{
struct dbDb *db = NULL;
int count = 0;
for(db = dbList; db != NULL; db = db->next)
    {
    if(sameString("Human", db->organism) && !strstrNoCase(db->name, "zoo"))
	count++;
    }
return count;
}


/* There are both from coordConv.c - simply copied over into here, slightly modified.  Should change include file after consultation...?*/

struct dbDb *loadDbInformation_mod(char *database)
/* load up the information for a particular draft */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row;
struct dbDb *dbList = NULL, *db = NULL;
char query[256];
snprintf(query, sizeof(query), "select * from dbDb where name='%s'", database);

/* Scan through dbDb table, loading into list */
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    db = dbDbLoad(row);
    slAddHead(&dbList, db);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
if(slCount(dbList) != 1)
    errAbort("coordConv.c::loadDbInformation() - expecting 1 dbDb record for %s got %d", db, slCount(dbList));
return dbList;
}


struct coordConvRep *createCoordConvRep_mod()
/* create a coordinate conversion report, allocating
   coordConv structures as well */
{
struct coordConvRep *ccr = NULL;
AllocVar(ccr);
AllocVar(ccr->from);
//
AllocVar(ccr->from->next);
AllocVar(ccr->to);
return ccr;
}

/* Very similar to the Non-Zoo except for Zoo "customizations" */

void getIndexedGenomeDescriptionsZoo(char ***retArray, int *retCount, boolean blatOnly)
/* Find out the list of genomes that have blat servers on them. */
{
struct dbDb *dbList = NULL, *db;
int i, count = 0;
char **array;
if(blatOnly)
    dbList = hGetBlatIndexedDatabases();
else
    dbList = hGetIndexedDatabases();

/* Call zooCount instead of human count */
count = zooCount(dbList);

if (count == 0)
    errAbort("No active %s servers in database", (blatOnly ? "blat" : "nib" ));
AllocArray(array, count);
i = 0;
for (db=dbList; db != NULL; db=db->next)
    {
    /* Get the proper Zoo species omitting combo */
     if(strstrNoCase(db->name, "zoo") && !strstrNoCase(db->name,"combo")){
    array[i++] = cloneString(db->description);
    }
    }
dbDbFreeList(&dbList);
*retArray = array;
*retCount = count;
}

void getIndexedGenomeDescriptions(char ***retArray, int *retCount, boolean blatOnly)
/* Find out the list of genomes that have blat servers on them. */
{
struct dbDb *dbList = NULL, *db;
int i, count = 0;
char **array;
if(blatOnly)
    dbList = hGetBlatIndexedDatabases();
else
    dbList = hGetIndexedDatabases();
count = humanCount(dbList);
if (count == 0)
    errAbort("No active %s servers in database", (blatOnly ? "blat" : "nib" ));
AllocArray(array, count);
i = 0;
for (db=dbList; db != NULL; db=db->next)
    {
    if(sameString("Human", db->organism) && !strstrNoCase(db->name, "zoo"))
	array[i++] = cloneString(db->description);
    }
dbDbFreeList(&dbList);
*retArray = array;
*retCount = count;
}



void appendWarningMsg(char *warning)
/* keep track of error messages for the user */
{
struct dyString *warn = newDyString(1024);
dyStringPrintf(warn, "%s", warning);
slAddHead(&webWarning, warn);
}

void posAbort(char *position)
/** print error message about format of position input */
{
char buff[256];
snprintf(buff, sizeof(buff), "Expecting position in the form chrN:10000-20000 got %s",
	 (position != NULL ? position : ""));
webAbort("Error:", buff );
}

void parsePosition(char *origPos, char **chr, int *s, int *e)
/** Parse the coordinate information from the user text */
{
/* trying to parse something that looks like chrN:10000-20000 */
char *pos = cloneString(origPos);
char *tmp = NULL;
char *tmp2 = NULL;
tmp = strstr(pos, ":");
if(tmp == NULL)
    posAbort(origPos);
*tmp='\0';
tmp++;
*chr = cloneString(pos);
tmp2 = strstr(tmp, "-");
if(tmp2 == NULL)
    posAbort(origPos);
*tmp2 = '\0';
tmp2++;
*s = atoi(stripCommas(tmp));
*e = atoi(stripCommas(tmp2));
}

static char *ccFreezeDbConversion(char *database, char *freeze, char *org)
/* Find freeze given database or vice versa.  Pass in NULL
 * for parameter that is unknown and it will be returned
 * as a result.  This result can be freeMem'd when done. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;
char *ret = NULL;
struct dyString *dy = newDyString(128);

if (database != NULL)
    dyStringPrintf(dy, "select description from dbDb where name = '%s' and (genome like '%s' or genome like 'Zoo')",
		   database, org);
else if (freeze != NULL)
    dyStringPrintf(dy, "select name from dbDb where description = '%s' and (genome like '%s' or genome like 'Zoo')",
		   freeze, org);
else
    internalErr();
sr = sqlGetResult(conn, dy->string);
if ((row = sqlNextRow(sr)) != NULL)
    ret = cloneString(row[0]);
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
freeDyString(&dy);
return ret;
}


void checkArguments()
/** setup our parameters depending on whether we've been called as a
    cgi script or from the command line */
{
hgTest = cgiBoolean("hgTest");
numTests = cgiOptionalInt("numTests",0);
origDb = cgiOptionalString("origDb");
origGenome = cgiOptionalString("origGenome");
newGenome = cgiOptionalString("newGenome");
position = cgiOptionalString("position");
chrom = cgiOptionalString("chrom");
chromStart = cgiOptionalInt("chromStart", -1);
chromEnd = cgiOptionalInt("chromEnd", -1);
calledSelf = cgiBoolean("calledSelf");

/* if we're testing we don't need to worry about UI errors */
if(hgTest)
    return;

/* parse the position string and make sure that it makes sense */
if (position != NULL && position[0] != 0)
    {
    parsePosition(cloneString(position), &chrom, &chromStart, &chromEnd);
    }
if (chromStart > chromEnd)
    {
    webAbort("Error:", "Start of range is greater than end. %d > %d", chromStart, chromEnd);
    }

/* convert the genomes requested to hgN format */
if(origGenome != NULL)
    origGenome = ccFreezeDbConversion(NULL, origGenome, organism);
if(newGenome != NULL)
    newGenome = ccFreezeDbConversion(NULL, newGenome, organism);

/* make sure that we've got valid arguments */
if((newGenome == NULL || origGenome == NULL || chrom == NULL || chromStart == -1 || chromEnd == -1) && (calledSelf))
    webAbort("Error:", "Missing some inputs.");

if( origGenome != NULL && sameString(origGenome, newGenome))
    {
    struct dyString *warning = newDyString(1024);
    dyStringPrintf(warning, "Did you really want to convert from %s to %s (the same genome)?",
		   ccFreezeDbConversion(origGenome, NULL, organism), \
		   ccFreezeDbConversion(newGenome, NULL, organism));
    appendWarningMsg(warning->string);
    dyStringFree(&warning);
    }
}

char *makeBrowserUrl(char *version, char*chrom, int start, int end)
/** create a url of the form hgTracks likes */
{
char url[256];
sprintf(url, "%sposition=%s:%d-%d&db=%s", browserUrl, chrom, start, end, version);
return cloneString(url);
}

void outputBlatLink(char *link, char *db, struct dnaSeq *seq)
/** output a link to hgBlat a certain sequence */
{
printf("<a href=\"%stype=DNA&db=%s&sort=query,score&output=hyperlink&userSeq=%s\">%s</a>",blatUrl, db,seq->dna, link);
}

void printWebWarnings()
/** print out any warning messages that we may have
 * for the user */
{
struct dyString *warn = NULL;
if(webWarning != NULL)
    {
    printf("<span style='color:red;'>\n");
    printf("<h3>Warning:</h3><ul>\n");
    for(warn = webWarning; warn != NULL; warn = warn->next)
	{
	printf("<li>%s</li>\n", warn->string);
	}
    printf("</ul></span>\n");
    }
}

void webOutFasta(struct dnaSeq *seq, char *db)
{
/** output a blat link and the fasta in cut and past form */
printf("<pre>\n");
faWriteNext(stdout, seq->name, seq->dna, seq->size);
printf("</pre>\n");
outputBlatLink("Blat Sequence on new Draft", db, seq);
printf("<br><br>");
}


void printSucessWarningZoo() {
/** Make sure the user knows to take this with a grain of salt */
printf("<p>Please be aware that this is merely our best guess of converting from one species to another. Make sure to check with local landmarks and use common sense.\n");
}

void printSucessWarning() {
/** Make sure the user knows to take this with a grain of salt */
printf("<p>Please be aware that this is merely our best guess of converting from one draft to another. Make sure to check with local landmarks and use common sense.\n");
}

void printTroubleShooting(struct coordConvRep *ccr)
{
/** print out the information used to try and convert */
webNewSection("Alignment Details:");
printf("<p>The following sequences from the original draft were aligned to determine the coordinates on the new draft:<br>\n");
webOutFasta(ccr->upSeq, ccr->to->version);
webOutFasta(ccr->midSeq, ccr->to->version);
webOutFasta(ccr->downSeq, ccr->to->version);
printf("<br><br>");
printf("<i><font size=-1>Comments, Questions, Bug Reports: <a href=\"mailto:sugnet@cse.ucsc.edu\">sugnet@cse.ucsc.edu</a></font></i>\n");
}

/* Mimic behaviour of doGoodReport do less stuff... */
void doGoodReportZoo(FILE *dummy, struct coordConvRep *ccr)
/** output the result of a successful conversion */
{
cartWebStart(cart, database, "Coordinate Conversion for %s %s:%d-%d",
	     ccr->from->date, ccr->from->chrom, ccr->from->chromStart, ccr->from->chromEnd);
printWebWarnings();
printf("<p><b>Success:</b> %s\n", ccr->msg);

printSucessWarningZoo();

printf("<ul><li><b>Original Coordinates:</b> %s %s:%d-%d  ",
       ccr->from->date ,ccr->from->chrom, ccr->from->chromStart, ccr->from->chromEnd);

printf("<a href=\"%s\">[browser]</a></li>\n",
       makeBrowserUrl(ccr->from->version, ccr->from->chrom, ccr->from->chromStart, ccr->from->chromEnd));

printf("<li><b>Region of Original Coordinates Converted:</b> %s %s:%d-%d  ",
       ccr->from->date ,ccr->from->chrom, ccr->from->next->chromStart, ccr->from->next->chromEnd);

printf("<a href=\"%s\">[browser]</a></li>\n",
       makeBrowserUrl(ccr->from->version, ccr->from->chrom, ccr->from->next->chromStart, ccr->from->next->chromEnd));

printf("<li><b>New Coordinates:</b> %s %s:%d-%d  ",
       ccr->to->date ,ccr->to->chrom, ccr->to->chromStart, ccr->to->chromEnd);

printf("<a href=\"%s\">[browser]</a></li></ul>\n",
       makeBrowserUrl(ccr->to->version, ccr->to->chrom, ccr->to->chromStart, ccr->to->chromEnd));

/* Trouble shooting isn't applicable in this case yet... ccr doesn't contain proper info.. */
/*  printTroubleShooting(ccr); */

cartWebEnd();
}

void doGoodReport(FILE *dummy, struct coordConvRep *ccr)
/** output the result of a successful conversion */
{
cartWebStart(cart, database, "Coordinate Conversion for %s %s:%d-%d",
	     ccr->from->date, ccr->from->chrom, ccr->from->chromStart, ccr->from->chromEnd);
printWebWarnings();
printf("<p><b>Success:</b> %s\n", ccr->msg);
if(sameString(ccr->midPsl->strand, "-"))
    {
    printf(" It appears that the orientation of your coordinate range has been inverted.\n");
    }
printSucessWarning();
printf("<ul><li><b>Old Coordinates:</b> %s %s:%d-%d  ",
       ccr->from->date ,ccr->from->chrom, ccr->from->chromStart, ccr->from->chromEnd);
printf("<a href=\"%s\">[browser]</a></li>\n",
       makeBrowserUrl(ccr->from->version, ccr->from->chrom, ccr->from->chromStart, ccr->from->chromEnd));
printf("<li><b>New Coordinates:</b> %s %s:%d-%d  ",
       ccr->to->date ,ccr->to->chrom, ccr->to->chromStart, ccr->to->chromEnd);
printf("<a href=\"%s\">[browser]</a></li></ul>\n",
       makeBrowserUrl(ccr->to->version, ccr->to->chrom, ccr->to->chromStart, ccr->to->chromEnd));
printTroubleShooting(ccr);
cartWebEnd();
}


/* Same as below.. pretty much */
void doBadReportZoo(FILE *dummy, struct coordConvRep *ccr)
/** output the result of a flawed conversion */{
cartWebStart(cart, database, "Coordinate Conversion for %s %s:%d-%d",
	     ccr->from->date, ccr->from->chrom, ccr->from->chromStart, ccr->from->chromEnd);
printWebWarnings();
printf("<p><b>Conversion Not Successful:</B> %s\n", ccr->msg);
printf("<p><a href=\"%s\">View old Coordinates in %s browser.</a>\n",
       makeBrowserUrl(ccr->from->version, ccr->from->chrom, ccr->from->chromStart, ccr->from->chromEnd),
       ccr->from->date);

/* Don't need this - would need to fill in ccr manually */
/* printTroubleShooting(ccr); */
cartWebEnd();
}

void doBadReport(FILE *dummy, struct coordConvRep *ccr)
/** output the result of a flawed conversion */{
cartWebStart(cart, database, "Coordinate Conversion for %s %s:%d-%d",
	     ccr->from->date, ccr->from->chrom, ccr->from->chromStart, ccr->from->chromEnd);
printWebWarnings();
printf("<p><b>Conversion Not Successful:</B> %s\n", ccr->msg);
printf("<p><a href=\"%s\">View old Coordinates in %s browser.</a>\n",
       makeBrowserUrl(ccr->from->version, ccr->from->chrom, ccr->from->chromStart, ccr->from->chromEnd),
       ccr->from->date);
printTroubleShooting(ccr);
cartWebEnd();
}


/* Version for Zoo species */
boolean convertCoordinatesZoo(FILE *goodOut, FILE *badOut,
			void (*goodResult)(FILE *out, struct coordConvRep *report),
			void (*badResult)(FILE *out, struct coordConvRep *report))
/* tries to convert coordinates and prints report
 depending on function pointers provided. In generial
 goodResult and badResult either generate html or tesxt
 if we are in cgi or testing mode respectively. */
{
struct blatServerTable *serve = NULL;
struct coordConvRep *ccr = createCoordConvRep_mod();
struct dbDb *newDbRec = NULL, *oldDbRec = NULL;
struct sqlConnection *conn = sqlConnect(origGenome);
struct linkedFeatures *lfList = NULL, *lf;
struct sqlResult *sr = NULL;

boolean success = FALSE;

/* Keeps track if we're in an inverted match or not */
boolean inversion = FALSE;

/* Two possible reasons two fail */
boolean incoherent = FALSE;
boolean max_apart= FALSE;

char track[256];
char success_message[256];
char **row;
int rowOffset;
int conv_total=0;
int iteration = 0;

/* These two distances check how different the distance is between the converted and unconverted coordinates.
   In this case if the distance between a converted versus unconverted block is more than 10 times
   and greater than 10 000 bases, set up a warning... */

int ref_end=0,ref_start,comp_end=0,comp_start=0;

/* Load info from databases into ccr */
oldDbRec = loadDbInformation_mod(origGenome);
ccr->from->chrom = cloneString(chrom);
ccr->from->chromStart = chromStart;
ccr->from->chromEnd = chromEnd;
ccr->from->version = cloneString(oldDbRec->name);
ccr->from->date = cloneString(oldDbRec->description);
ccr->from->nibDir = cloneString(oldDbRec->nibPath);
ccr->seqSize=1000;
newDbRec = loadDbInformation_mod(newGenome);
ccr->to->version = cloneString(newDbRec->name);
ccr->to->date = cloneString(newDbRec->description);
ccr->to->nibDir = cloneString(newDbRec->nibPath);
ccr->good=FALSE;

/* Create the correct track name...  Will have to be changed when multiple versions? */

sprintf(track,"%s_%s",origGenome,newGenome);

/* Get the information from loading the track. */
/* Double check we are not using a track connecting 1 and 2 */

if(!(strstr(track,"2") && strstr(track,"1")))
    {
    sr = hRangeQuery(conn, track, chrom, chromStart, chromEnd, NULL, &rowOffset);
    }

while ((row = sqlNextRow(sr)) != NULL)
    {
    /* Find the correponding track */
    struct psl *psl = pslLoad(row+rowOffset);

    /* If first time through... */
    if(iteration==0)
	{
	/* Fill in stuff if first time through... */
	ccr->to->chrom=cloneString(psl->qName);
	ccr->to->chromStart=psl->qStart;

	/* Actual point of conversion of coordinates */
	ccr->from->next->chromStart=psl->tStart;
	ccr->good=TRUE;

	success=TRUE;
	}

    /* check for erroneous conversion if not first time through */
    /* Check for inversions, massive insertions... */

    /* Check for inversion (old start is "bigger" than new start)*/

    if(iteration > 0)
	{
	if((comp_start> psl->qStart))
	    {
	    /* If not currently in an inversion state */
	    if(!inversion )
		/* If not the second time through (first time inversion could be detected) */
		if(iteration > 2)
		    incoherent=TRUE;

	    /* Reset variables used for measuring distance... */

	    /* Set inversion state variable to true */
	    inversion = TRUE;


	    /* Check to see if there are too great distances ... */

	    if( ((comp_start - psl->qEnd)>(10 * (psl->tStart - ref_end))) && ((comp_start - psl->qEnd) > 10000))
		max_apart=TRUE;
	    }
	else
	    /* No inversion */
	    {
	    /* Check if previous state was an inversion (then flip flop)...*/
	    if(inversion)
		incoherent = TRUE;
	    else
		{
		/* Check to see if the mapping is too far apart */
		if( ((psl->qStart - comp_end) > (10 * (psl->tStart - ref_end))) && ((psl->qStart - comp_end) > 10000))
		    max_apart=TRUE;
		}
	    }
	}

    if(inversion)
	{
	if(iteration == 1)
	    ccr->to->chromEnd=comp_end;

	ccr->to->chromStart=psl->qStart;
	}
    else
	ccr->to->chromEnd=psl->qEnd;

    ccr->from->next->chromEnd=psl->tEnd;

    if(max_apart || incoherent)
	{
	success=FALSE;
	break;
	}

    if(psl->tStart > ref_end)
	conv_total+=(psl->tEnd - psl->tStart);
    else
	conv_total+=(psl->tEnd - ref_end);

    ref_end=psl->tEnd;
    comp_end=psl->qEnd;
    ref_start=psl->tStart;
    comp_start=psl->qStart;

    iteration++;
    pslFree(&psl);
    }

if(!success)
    {
    /* Check to see if using version two of zoo.  Not integrated into the database at this stage... */
    if((strstr(origGenome,"2") && strstr(newGenome,"1"))|| (strstr(newGenome,"2") && strstr(origGenome,"1")))
	sprintf(success_message,"Couldn't convert between these two genomes since the cross conversion between the two zoo dataset hasn't been fully integrated into the database");
    else if (max_apart)
	sprintf(success_message, "Coordinates couldn't reliably be converted between the two species.  Try using a smaller window. ");
    else if (incoherent)
	sprintf(success_message, "Coordinates couldn't be converted due to inconsistent inversions.");
    else
	sprintf(success_message,"Couldn't find a corresponding region for the original genome to the new genome.");

    ccr->msg=cloneString(success_message);
    badResult(badOut,ccr);
    }
else
    {
    sprintf(success_message,"Successfully converted (%3.1f%% of the original region was converted.)",((float)(conv_total * 100))/(float)(chromEnd-chromStart));
    ccr->msg=cloneString(success_message);
    goodResult(goodOut,ccr);
    }

dbDbFree(&oldDbRec);
dbDbFree(&newDbRec);
coordConvRepFreeList(&ccr);
return success;
}


boolean convertCoordinates(FILE *goodOut, FILE *badOut,
			void (*goodResult)(FILE *out, struct coordConvRep *report),
			void (*badResult)(FILE *out, struct coordConvRep *report))
/* tries to convert coordinates and prints report
 depending on function pointers provided. In generial
 goodResult and badResult either generate html or tesxt
 if we are in cgi or testing mode respectively. */
{
struct blatServerTable *serve = NULL;
struct coordConvRep *ccr = NULL;
boolean success = FALSE;
serve = hFindBlatServer(newGenome, FALSE);
ccr = coordConvConvertPos(chrom, chromStart, chromEnd, origGenome, newGenome,
	       	       serve->host, serve->port, serve->nibDir);
if(ccr->good)
    {
    goodResult(goodOut, ccr);
    success = TRUE;
    }
else
    {
    badResult(badOut, ccr);
    success = FALSE;
    }
coordConvRepFreeList(&ccr);
return success;
}

void doConvertCoordinates(struct cart *theCart)
/* tries to convert the coordinates given */
{
/* Seperate zoo conversions from non-zoo conversions... */

cart = theCart;
hgBotDelay();	/* Prevent abuse. */

if(strstr(origGenome, "zoo")){
    convertCoordinatesZoo(stdout, stdout, doGoodReportZoo, doBadReportZoo);
}
else
    convertCoordinates(stdout, stdout, doGoodReport, doBadReport);
}


char *chooseDb(char *db1, char *db2)
/* match up our possible databases with the date version i.e. Dec 17, 2000 */
{
int i;
if(db1 != NULL)
    {
    if(strstr(db1, "hg") == db1)
	return ccFreezeDbConversion(db1, NULL, organism);
    else
	return db1;
    }
else
    {
    if(strstr(db2, "hg") == db2)
	return ccFreezeDbConversion(db2, NULL, organism);
    else
	return db2;
    }
return NULL;
}

/* This is very similar to doForm except it's for Zoo species */
void doFormZoo(struct cart *lCart)
/** Print out the form for users */
{
char **genomeList = NULL;
int genomeCount = 0;
char *dbChoice = NULL;
int i = 0;
cart = lCart;
cartWebStart(cart, databases, "Converting Coordinates Between Species");
puts(
     "<p>This page attempts to convert coordinates from one Zoo species' CFTR region\n"
     "to another. The mechanism for doing this is to use the blastz alignments which have been\n"
     "done between each Zoo species and use this to convert coordinates.  In general these should\n"
     "be reasonable conversions however the user is advised to not take the conversions as absolute \n"
     "standards.  Furthermore a given region from a given species is not necessarily alignable to any\n"
     "region in a different species.\n"
     );

/* Get all Zoo species since we are using blastz static alignments */
getIndexedGenomeDescriptionsZoo(&genomeList, &genomeCount,FALSE);

/* choose whether to use the db supplied by cgi or our default */
if(origDb != NULL && strstr(origDb, "zoo") == NULL)
    errAbort("Sorry, this mode of the program only works between zoo species.");
dbChoice = chooseDb(origDb, genomeList[0]);

printf("<form action=\"../cgi-bin/hgCoordConv\" method=get>\n");
printf("<br><br>\n");
printf("<table><tr>\n");
printf("<b><td><table><tr><td>Original Species: </b>\n");
cgiMakeDropList("origGenome", genomeList, genomeCount, dbChoice);
printf("</td></tr></table></td>\n");
printf("  <b><td><table><tr><td>Original Position:  </b>\n");

/* if someone has passed in a position fill it in for them */
if(position == NULL)
    cgiMakeTextVar("position",defaultPos, 30);
else
    cgiMakeTextVar("position",position, 30);
printf("</td></tr></table></td>\n");
printf("<b><td><table><tr><td>New Species: </b>\n");

freez(&genomeList);
genomeCount =0;
getIndexedGenomeDescriptionsZoo(&genomeList, &genomeCount, FALSE);
cgiMakeDropList("newGenome", genomeList, genomeCount, genomeList[genomeCount -1]);
printf("</td></tr></table></td></tr>\n");
printf("<tr><td colspan=6 align=right><br>\n");
cgiMakeButton("Submit","submit");
printf("</center></td></tr></table>\n");
cgiMakeHiddenVar("calledSelf", "on");
printf("</form>\n");
cartWebEnd();
}


void doForm(struct cart *lCart)
/** Print out the form for users */
{
char **genomeList = NULL;
int genomeCount = 0;
char *dbChoice = NULL;
int i = 0;
cart = lCart;
cartWebStart(cart, databases, "Converting Coordinates Between Drafts");
puts(
     "<p>This page attempts to convert coordinates from one draft of the human genome\n"
     "to another. The mechanism for doing this is to cut out and align pieces from the\n"
     "old draft and align them to the new draft making sure that\n"
     "they are in the same order and orientation. In general the smaller the sequence the better\n"
     "the chances of successful conversion.\n"
     );

getIndexedGenomeDescriptions(&genomeList, &genomeCount, TRUE);

/* choose whether to use the db supplied by cgi or our default */
if(origDb != NULL && strstr(origDb, "hg") == NULL)
    errAbort("Sorry, currently the conversion program only works with human genomes.");
dbChoice = chooseDb(origDb, genomeList[0]);

printf("<form action=\"../cgi-bin/hgCoordConv\" method=get>\n");
printf("<br><br>\n");
printf("<table><tr>\n");
printf("<b><td><table><tr><td>Original Draft: </b>\n");
cgiMakeDropList("origGenome", genomeList, genomeCount, dbChoice);
printf("</td></tr></table></td>\n");
printf("  <b><td><table><tr><td>Original Position:  </b>\n");

/* if someone has passed in a position fill it in for them */
if(position == NULL)
    cgiMakeTextVar("position",defaultPos, 30);
else
    cgiMakeTextVar("position",position, 30);
printf("</td></tr></table></td>\n");
printf("<b><td><table><tr><td>New Draft: </b>\n");

freez(&genomeList);
genomeCount =0;
getIndexedGenomeDescriptions(&genomeList, &genomeCount, TRUE);
cgiMakeDropList("newGenome", genomeList, genomeCount, genomeList[genomeCount -1]);
printf("</td></tr></table></td></tr>\n");
printf("<tr><td colspan=6 align=right><br>\n");
cgiMakeButton("Submit","submit");
printf("</center></td></tr></table>\n");
cgiMakeHiddenVar("calledSelf", "on");
printf("</form>\n");
cartWebEnd();
}

/* from here until main() is for testing */

unsigned getChromSize(char *chrom, char *db)
/* need size of a chromosome so we don't pick random
   probes that are off one end */
{
char chromSize[512];
char query[512];
struct sqlConnection *conn = sqlConnect(db);
sprintf(query, "select size from chromInfo where chrom = '%s'", chrom);
sqlQuickQuery(conn, query, chromSize, sizeof(chromSize));
sqlDisconnect(&conn);
assert(chromSize != NULL);
return atoi(chromSize);
}

char *getRandChrom()
/* randomly generate a chromosome */
{
int r = rand();
int num = (int)(21.0*r/(RAND_MAX+1.0));
char chrom[16];
num++;
sprintf(chrom,"chr%d",num);
return cloneString(chrom);
}

struct coordConv *getRandomCoord(char *database)
/* randomly generate a chromosome coordinate */
{
char *chrom = NULL;
unsigned chromSize = 0;
struct coordConv *cc = NULL;
int start=0,end=0, rest=0;
chrom = getRandChrom();
chromSize = getChromSize(chrom, database);
start = (rand() % chromSize);
rest = chromSize - start;
if(rest > 100000) rest = 100000;
end = start + (rand() % rest);
AllocVar(cc);
cc->chrom = cloneString(chrom);
cc->chromStart = start;
cc->chromEnd = end;
cc->version = cloneString(database);
return cc;
}

void putTic()
/* put a tic out for user feedback */
{
printf(".");
fflush(stdout);
}

void printReport(FILE *f, struct coordConvRep *ccr)
/* print out a text report of differences from start and stop */
{
int chrom=0, startDiff=0, endDiff=0;
if(sameString(ccr->from->chrom, ccr->to->chrom))
    chrom=1;
else chrom=0;

startDiff = ccr->from->chromStart - ccr->to->chromStart;
endDiff = ccr->from->chromEnd - ccr->to->chromEnd;
/* want to keep track of when we called correct but
   were wrong, and called incorrect and were wrong */
if(startDiff == 0 && endDiff == 0)
    if(ccr->good)
	hgTestCorrect++;
    else
	hgTestWrong++;
else
    if(ccr->good)
	hgTestWrong++;
    else
	hgTestCorrect++;
fprintf(f, "%d\t%d\t%d\t", chrom, startDiff, endDiff);
coordConvOutput(ccr->from, f, '\t', ';');
coordConvOutput(ccr->to, f, '\t', ';');
fprintf(f,"\t%s\n",ccr->msg);
fflush(f);
}

void runSamples(char *goodFile, char *badFile, char *newDb, char *oldDb, int numToRun)
/* run a bunch of tests */
{
int i,j,k;
FILE *good = mustOpen(goodFile, "w");
FILE *bad = mustOpen(badFile, "w");
char *tmp = NULL;
int numGood=0, numBad=0, tooManyNs=0;
boolean success = FALSE;
struct dnaSeq *seq = NULL;
printf("Running Tests\t");
for(i=0;i<numToRun;i++)
    {
    struct coordConvRep *ccr = NULL;
    struct coordConv *cc = NULL;
    if(!(i%10)) putTic();
    cc = getRandomCoord(oldDb);
    seq = hDnaFromSeq(cc->chrom, cc->chromStart, cc->chromEnd, dnaLower);
    if(!(strstr(seq->dna, "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn")))
	{
	chrom = cc->chrom;
	chromStart = cc->chromStart;
	chromEnd = cc->chromEnd;
	success = convertCoordinates(good, bad, printReport, printReport);
	if(success)
	    numGood++;
	else
	    numBad++;
	}
    else
	{
	tooManyNs++;
	}
    freeDnaSeq(&seq);
    coordConvFree(&cc);
    }
carefulClose(&good);
carefulClose(&bad);
printf("\tDone.\n");
printf("Out of %d attempts got %d 'succesfully converted' and %d 'had problems', %d had too many N's\n",
       (numGood + numBad), numGood, numBad, tooManyNs);
printf("After checking got %d of %d correctly called and %d incorrectly called.\n",
       hgTestCorrect, hgTestCorrect+hgTestWrong, hgTestWrong);
}

int main(int argc, char *argv[])
{
if(argc == 1 && !cgiIsOnWeb())
    usage();
cgiSpoof(&argc, argv);
checkArguments();
hSetDb(origGenome);
if(hgTest)
    runSamples("hgCoordConv.test.good", "hgCoordConv.test.bad", origGenome, origGenome, numTests);
else
    {
/* do our thing  */
    if(calledSelf)
        {
	cartEmptyShell(doConvertCoordinates, hUserCookie(), excludeVars, NULL);
        }
    else
        {
        /* Check to see if in zoo browser... if so call doFormZoo */
        if (!containsStringNoCase(origDb, "zoo"))
	cartEmptyShell(doForm, hUserCookie(), excludeVars, NULL);
    else
	cartEmptyShell(doFormZoo, hUserCookie(), excludeVars, NULL);
        }
    }
return 0;
}
