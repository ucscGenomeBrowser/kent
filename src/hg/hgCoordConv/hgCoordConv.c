/**
   cgi program to convert coordinates from one draft of the 
   genome to another. Cool place to test is: chr1:126168504-126599722
   from Dec to April, it was inverted. Looks to be inverted back in
   Aug.
*/
#include "common.h"
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

int humanCount(struct dbDb *dbList)
/* Count the number of human organism records. */
{
struct dbDb *db = NULL;
int count = 0;
for(db = dbList; db != NULL; db = db->next)
    {
    if(sameString("Human", db->organism))
	count++;
    }
return count;
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
    if(sameString("Human", db->organism))
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
*s = atoi(tmp);
*e = atoi(tmp2);
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
    origGenome = hDbFromFreeze(origGenome);
if(newGenome != NULL) 
    newGenome = hDbFromFreeze(newGenome);

/* make sure that we've got valid arguments */
if((newGenome == NULL || origGenome == NULL || chrom == NULL || chromStart == -1 || chromEnd == -1) && (calledSelf)) 
    webAbort("Error:", "Missing some inputs.");

if( origGenome != NULL && sameString(origGenome, newGenome))
    {
    struct dyString *warning = newDyString(1024);
    dyStringPrintf(warning, "Did you really want to convert from %s to %s (the same genome)?", 
		   hFreezeFromDb(origGenome), hFreezeFromDb(newGenome));
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
printf("<a href=\"%stype=DNA&genome=%s&sort=query,score&output=hyperlink&userSeq=%s\">%s</a>",blatUrl, db,seq->dna, link);
}

void printWebWarnings() 
/** print out any warning messages that we may have
 * for the user */
{
struct dyString *warn = NULL;
if(webWarning != NULL)
    {
    printf("<font color=red>\n");
    printf("<h3>Warning:</h3><ul>\n");
    for(warn = webWarning; warn != NULL; warn = warn->next)
	{
	printf("<li>%s</li>\n", warn->string);
	}
    printf("</ul></font>\n");
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

void doGoodReport(FILE *dummy, struct coordConvRep *ccr) 
/** output the result of a successful conversion */
{
cartWebStart("Coordinate Conversion for %s %s:%d-%d", 
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

void doBadReport(FILE *dummy, struct coordConvRep *ccr) 
/** output the result of a flawed conversion */{
cartWebStart("Coordinate Conversion for %s %s:%d-%d", 
	     ccr->from->date, ccr->from->chrom, ccr->from->chromStart, ccr->from->chromEnd);
printWebWarnings();
printf("<p><b>Conversion Not Successful:</B> %s\n", ccr->msg);
printf("<p><a href=\"%s\">View old Coordinates in %s browser.</a>\n", 
       makeBrowserUrl(ccr->from->version, ccr->from->chrom, ccr->from->chromStart, ccr->from->chromEnd),
       ccr->from->date);
printTroubleShooting(ccr);
cartWebEnd();
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

void doConvertCoordinates()
/* tries to convert the coordinates given */
{
convertCoordinates(stdout, stdout, doGoodReport, doBadReport);
}


char *chooseDb(char *db1, char *db2)
/* match up our possible databases with the date version i.e. Dec 17, 2000 */
{
int i;
if(db1 != NULL) 
    {
    if(strstr(db1, "hg") == db1)
	return hFreezeFromDb(db1);
    else
	return db1;
    }
else 
    {
    if(strstr(db2, "hg") == db2)
	return hFreezeFromDb(db2);
    else
	return db2;
    }
return NULL;
}


void doForm(struct cart *lCart) 
/** Print out the form for users */
{
char **genomeList = NULL;
int genomeCount = 0;
char *dbChoice = NULL;
int i = 0;
cart = lCart;
cartWebStart("Converting Coordinates Between Drafts");
puts( 
     "<p>This page attempts to convert coordinates from one draft of the human genome\n"
     "to another. The mechanism for doing this is to cut out and align pieces from the\n"
     "old draft and align them to the new draft making sure that\n"
     "they are in the same order and orientation. In general the smaller the sequence the better\n"
     "the chances of successful conversion.\n"
     );

getIndexedGenomeDescriptions(&genomeList, &genomeCount, FALSE);

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
	cartEmptyShell(doConvertCoordinates, hUserCookie(), excludeVars, NULL);
    else
	cartEmptyShell(doForm, hUserCookie(), excludeVars, NULL);
    }
return 0;
}
