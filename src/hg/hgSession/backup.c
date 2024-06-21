/* backupRestore - Routines related to backing up and restoring the current cart and its custom tracks. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */


#include "common.h"

#include "cart.h"
#include "net.h"
#include "textOut.h"
#include "base64.h"
#include "md5.h"
#include "obscure.h"
#include "net.h"
#include "hgConfig.h"

#include "htmshell.h"
#include "cheapcgi.h"
#include "hgSession.h"

#include <sys/wait.h>

#include "hCommon.h"
#include "hdb.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "customFactory.h"
#include "jsHelper.h"
#include "trashDir.h"
#include "filePath.h"
#include "cgiApoptosis.h"

#include "wiggle.h"

#include "pipeline.h"

#define UCSC_GB_BACKUP_VERSION_FILENAME "UCSC_GB_BACKUP_VERSION"


static void errAbortHandler(char *format, va_list args)
{
// provide more explicit message when we run out of memory (#5147).
if(strstr(format, "needLargeMem:") || strstr(format, "carefulAlloc:"))
    htmlVaWarn("Region selected is too large for calculation. Please specify a smaller region or try limiting to fewer data points.", args);
else
    {
    // call previous handler
    popWarnHandler();
    vaWarn(format, args);
    }
if(isErrAbortInProgress())
    noWarnAbort();
}

static void vaHtmlOpen(char *format, va_list args)
/* Start up a page that will be in html format. */
{
puts("Content-Type:text/html\n");
cartVaWebStart(cart, database, format, args);
pushWarnHandler(errAbortHandler);
}

void htmlOpen(char *format, ...)
/* Start up a page that will be in html format. */
{
va_list args;
va_start(args, format);
vaHtmlOpen(format, args);
va_end(args);
}

void htmlClose()
/* Close down html format page. */
{
popWarnHandler();
cartWebEnd();
}

// ------------------------------------------

boolean waitForBackgroundFile(char *path)
/* Wait 30 seconds for the file at path to appear. Return TRUE if timeout exceeded. */
{
int waited = 0;
int waitLimit = 30;   // max wait in seconds
// sometimes the background process is a little slow,
// we can wait up to 30 seconds for it.
while (!(fileExists(path) && fileSize(path) > 0) && (waited < waitLimit))
    {
    sleep(1);
    ++waited;
    }
if (waited >= waitLimit)
    {
    htmlOpen("Background Status");
    errAbort("Background file %s not found or empty.", path);
    htmlClose();
    return TRUE;
    }
return FALSE;
}

void getBackgroundStatus(char *url)
/* fetch status as the latest complete html block available.
 * fetch progress info instead if background proc still running. */
{
char *html = NULL;

// get the pid and see if it is still running
char pidName[1024];
safef(pidName, sizeof pidName, "%s.pid", url);

if (waitForBackgroundFile(pidName)) return;

FILE *pidF = mustOpen(pidName, "r");
long pid = 0;
fscanf(pidF, "%ld", &pid);
carefulClose(&pidF);

// If the background process is still running, grab the progress file instead
if (getpgid(pid) >= 0)
    {
    htmlOpen("Background Status");
    char progressName[1024];
    safef(progressName, sizeof progressName, "%s.progress", url);
    if (waitForBackgroundFile(progressName)) return;
    if (fileExists(progressName))
	{
	readInGulp(progressName, &html, NULL);
	printf("%s",html);
	}
    else
	{
	printf("progress file missing<br>\n");
	}

    printf("<BR>\n<FORM ACTION=\"/cgi-bin/hgTables\" METHOD=GET>\n"
	//"<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"Back\" >"
	"<INPUT TYPE=SUBMIT NAME=\"Refresh\" id='Refresh' VALUE=\"Refresh\">"
	"</FORM>\n"
	);
    jsOnEventById("click", "Refresh", "window.location=window.location;return false;");
    jsInline("setTimeout(function(){location = location;},5000);\n");
    jsInline("window.scrollTo(0,document.body.scrollHeight);");

    htmlClose();
    return;
    }

// otherwise read the main html file
// sometimes the background process is a little slow,
if (waitForBackgroundFile(url)) return;

readInGulp(url, &html, NULL);
int numLines = chopString(html, "\n", NULL, 1000000);
char **lines = NULL;
AllocArray(lines, numLines);
chopString(html, "\n", lines, numLines);
int end;
for (end=numLines-1; end >= 0 && ! (endsWith(lines[end], "</html>") || endsWith(lines[end], "</HTML>")) ; --end)
    /* do nothing */ ;
if (end < 0)
    {
    htmlOpen("Background Status");
    errAbort("No complete html found");
    htmlClose();
    return;
    }
int start;
for (start=end; start >= 0 && ! (startsWith("<html>", lines[start]) || startsWith("<HTML>", lines[start])) ; --start)
    /* do nothing */ ;
if (start < 0)
    {
    htmlOpen("Background Status");
    errAbort("No html start tag found");
    htmlClose();
    return;
    }
puts("Content-Type: text/html\n");
int line;
boolean autoRefreshFound = FALSE;
boolean successfullyUploaded = FALSE;
for (line=start; line <= end; line++)
    {
    puts(lines[line]);
    if (startsWith("setTimeout(function(){location = location;}", lines[line]))
	autoRefreshFound = TRUE;
    if (startsWith("Output has been successfully uploaded", lines[line]))
	successfullyUploaded = TRUE;
    }
// if it looks like the background is no longer running, 
// include the .err stdout output for more informative problem message
char urlErr[512];
char *textErr = NULL;
safef(urlErr, sizeof urlErr, "%s.err", url);
if (!autoRefreshFound && !successfullyUploaded && (fileSize(urlErr) > 0))
    {
    readInGulp(urlErr, &textErr, NULL);
    //printf("%s", textErr); /* more annoying than helpful in some places. */
    }
}



void startBackgroundWork(char *exec, char **pWorkUrl)
/* deal with forking off child for background work
 * and setting up the trash file for communicating
 * from the child to the browser */
{
char *workUrl = NULL;
char hgsid[64];
struct tempName tn;
safef(hgsid, sizeof(hgsid), "%s", cartSessionId(cart));
trashDirFile(&tn, "backGround", hgsid, ".tmp");
workUrl = cloneString(tn.forCgi);
fflush(stdout);
fflush(stderr);
// seems that we need to use the double-fork trick
// to create enough separation between the non-waiting parent
// and the grand-child process.  otherwise the OS and Apache are waiting on the child.

int pid = fork();
if (pid == -1)
    {   
    errAbort("can't fork, error %d", errno);
    }
if (pid == 0) // child
    {
    int pid2 = fork();
    if (pid2 == -1)
	{   
	errAbort("can't fork, error %d", errno);
	}
    if (pid2 == 0) // grand child
	{

	// we need to close or redup to open stdout, stderr, stdin
	// in order for apache to break ties with it.
	// Will the grandchild cgi still be able to function?

	// redirect stdout of child to the trash file for easier use of 
	// library functions that output html to stdout.
	int out = mustOpenFd(tn.forCgi, O_WRONLY | O_CREAT);
	fflush(stdout);
	dup2(out,STDOUT_FILENO);  /* closes STDOUT before setting it back to saved descriptor */
	close(out);

	// Unfortunately we must create our own stderr log file
	char errName[1024];
	safef(errName, sizeof errName, "%s.err", tn.forCgi);
	int err = mustOpenFd(errName, O_CREAT | O_WRONLY | O_APPEND);
	dup2(err, STDERR_FILENO);
	close(err);

	// stdin input is just empty
	int in = mustOpenFd("/dev/null", O_RDONLY);
	dup2(in, STDIN_FILENO);
	close(in);

	// save own pid in .pid file
        long selfPid = (long) getpid();
	char pidName[1024];
	safef(pidName, sizeof pidName, "%s.pid", tn.forCgi);
	char pidBuf[64];
	safef(pidBuf, sizeof pidBuf, "%ld", selfPid);
	int pidFd = mustOpenFd(pidName, O_CREAT | O_WRONLY | O_APPEND);
	mustWriteFd(pidFd, pidBuf, strlen(pidBuf));
	close(pidFd);

	// progress is a separate channel for progress update
	char progressName[1024];
	safef(progressName, sizeof progressName, "%s.progress", tn.forCgi);

	// execute so that we will be able to use database and other operations normally.
	char execPath[4096];
	safef(execPath, sizeof execPath, "%s hgsid=%s backgroundProgress=%s", exec, hgsid, progressName);
	char *args[10];
	int numArgs = chopString(execPath, " ", args, 10);
	args[numArgs] = NULL;
	// by creating a minimal environment and not inheriting from the parent,
	// it cause cgiSpoof to run,  picking up command-line params as cgi vars.
	// SERVER_SOFTWARE triggers default cgi trash temp location
	char *newenviron[] = { "HGDB_CONF=hg.conf", "SERVER_SOFTWARE=Apache", NULL };
	int sleepSeconds = 1; // was 5
	sleep(sleepSeconds); // Give the foreground process time to write the cart.
	execve(args[0], args+1, newenviron);
	// SHOULD NOT GET HERE UNLESS EXEC FAILED.
	verbose(1,"execve failed for %s\n", exec);
	_exit(0); // exit without the usual cleanup which messes up parent's db connections etc.

	}
    else  // child
	{
	_exit(0); // exit without the usual cleanup which messes up parent's db connections etc.
	}
    }
else  // parent
    {
    *pWorkUrl = workUrl;
    // wait for the exiting child (not grandchild)
    int w, status;
    do {
	w = waitpid(pid, &status, WUNTRACED | WCONTINUED);
	if (w == -1) 
	    {
	    perror("waitpid");
	    exit(EXIT_FAILURE);
	    }

	if (WIFEXITED(status)) 
	    {
	    if (WEXITSTATUS(status) != 0)
		verbose(1, "exited, status=%d\n", WEXITSTATUS(status));
	    } 
	else if (WIFSIGNALED(status)) 
	    {
	    verbose(1, "killed by signal %d\n", WTERMSIG(status));
	    } 
	else if (WIFSTOPPED(status)) 
	    {
	    verbose(1, "stopped by signal %d\n", WSTOPSIG(status));
	    }
	else if (WIFCONTINUED(status)) 
	    {
	    verbose(1, "continued\n");
	    }
	} while (!WIFEXITED(status) && !WIFSIGNALED(status));

        // done waiting for child.

    }

}



boolean isActiveDb(char *db)
/* check if the db is in dbDb and active */
{
struct sqlConnection *conn = hConnectCentral();
char query[256];
sqlSafef(query, sizeof query, "select count(*) from dbDb where name='%s' and active=1", db);
int count = sqlQuickNum(conn, query);
hDisconnectCentral(&conn);
return (count >= 1);
}

unsigned isLiveHub(char *db, char **pUrl, boolean talkative)
{
unsigned hubId = hubIdFromTrackName(db);
struct sqlConnection *conn = hConnectCentral();
struct hubConnectStatus *hubStat = hubConnectStatusForId(conn, hubId);
hDisconnectCentral(&conn);
unsigned result = 0;
if (hubStat)
    {
    if (hubStat->errorMessage)
	{
	if (talkative)
	    printf("HUB errMessage=%s<BR>\n", hubStat->errorMessage);
	}
    else
	{

	if (hubStat->trackHub)
	    {
	    if (trackHubHasDatabase(hubStat->trackHub, db))
		{
		if (pUrl)
		    *pUrl = cloneString(hubStat->hubUrl);
		result = hubId;  // indicate db found in hub.
		}
	    if (talkative)
		printf("[%s]<br>\n", hubStat->trackHub->shortLabel); 
	    }
	}
	
    }
return result;
}

char *getHubUrlFromId(unsigned hubId)
{
struct sqlConnection *conn = hConnectCentral();
struct hubConnectStatus *hubStat = hubConnectStatusForId(conn, hubId);
hDisconnectCentral(&conn);
char *result = NULL;
if (hubStat)
    {
    if (!hubStat->errorMessage)
	{
	if (hubStat->trackHub)
	    {
	    result = cloneString(hubStat->hubUrl);
	    }
	}
    }
return result;
}


/* hold extra data that does not fit in the track struct itself,
 * but which is track-specific */
struct ctExtra
    {
    struct ctExtra *next;
    char *name;  // important to identity of the track in hgCustom
    char *browserLines;
    char *trackLine;
    int tableRows;
    unsigned long tableDataLength;
    char *bigDataUrl;
    };

struct downloadResults
    {
    struct downloadResults *next;
    char *db;
    char *ctPath;    // path to ct file in trash
    struct customTrack *cts;  // tracks
    struct ctExtra *ctExtras; // parallel additional info to the tracks
    };


void addFieldToTrackLine(struct dyString *dy, struct customTrack *track, char *field, boolean isRequired)
/* add optional or required field and its value to a track line */
{
char *val = hashFindVal(track->tdb->settingsHash, field);
if (val)
    {
    dyStringPrintf(dy, " %s='%s'", field, val);
    }
else
    {
    if (isRequired)
	errAbort("required field %s is missing from custom track settings", field);
    }
}

char *fabricateWigTrackline(struct customTrack *track)
/* make up a missing trackline for a ct exported from Table Browser as a wig */
{
struct dyString *dy = dyStringNew(1024);
dyStringPrintf(dy, "track ");
addFieldToTrackLine(dy, track,"name", TRUE);  // Required
dyStringPrintf(dy, " type='wiggle_0'");
addFieldToTrackLine(dy, track,"description", FALSE);
addFieldToTrackLine(dy, track,"visibility", FALSE);
addFieldToTrackLine(dy, track,"priority", FALSE);
addFieldToTrackLine(dy, track,"altColor", FALSE);
return dyStringCannibalize(&dy);
}
 

struct downloadResults *processCtsForDownloadInternals(char *contents, char **pTrackHubsVar)
/* Process the saved session cart contents,
 * looking for custom-tracks which are db-specific. */
{

struct downloadResults *resultList = NULL;

if (!contents)
    return resultList;

char *contentsToChop = cloneString(contents);
char *namePt = contentsToChop;

struct sqlConnection *ctConn = hAllocConn(CUSTOM_TRASH);

while (isNotEmpty(namePt))
    {
    char *dataPt = strchr(namePt, '=');
    char *nextNamePt;
    if (dataPt == NULL)
	errAbort("ERROR: Mangled session content string %s", namePt);
    *dataPt++ = 0;
    nextNamePt = strchr(dataPt, '&');
    if (nextNamePt != NULL)
	*nextNamePt++ = 0;
    if (startsWith(CT_FILE_VAR_PREFIX, namePt))
	{
	cgiDecode(dataPt, dataPt, strlen(dataPt));

	struct downloadResults *result = NULL;
	AllocVar(result);

	char *db = namePt + strlen(CT_FILE_VAR_PREFIX);
	result->db = cloneString(db);

	if (fileExists(dataPt))
	    {

	    result->ctPath = cloneString(dataPt);
	
	    struct customTrack *cts = NULL, *track = NULL;
	    boolean thisGotLiveCT = FALSE, thisGotExpiredCT = FALSE;

	    customFactoryTestExistence(db, dataPt, &thisGotLiveCT, &thisGotExpiredCT, &cts);

	    result->cts = cts;

	    //  add it to a structure for tracking/printing later.

	    for (track=cts; track; track = track->next)
		{
		struct ctExtra *extra = NULL;
		AllocVar(extra);

		// what to save in ct output

		// name is important because hgCustom identifies by this string
		char *name = hashFindVal(track->tdb->settingsHash, "name");
		extra->name = cloneString(name);

                char *browserLines = hashFindVal(track->tdb->settingsHash, "browserLines");
		// to handle multiple-lines replace semi-colon ";" with newline.
		if (browserLines)
		    replaceChar(browserLines,';','\n');	
		extra->browserLines = cloneString(browserLines);

                char *origTrackLine = hashFindVal(track->tdb->settingsHash, "origTrackLine");
		extra->trackLine = cloneString(origTrackLine);
		if (!extra->trackLine) // Table Browser creates some wiggles without an origTrackLine
		    {
		    char *tdbType = hashFindVal(track->tdb->settingsHash, "tdbType");
		    if (tdbType && startsWith("wig ", tdbType))
			{
			extra->trackLine = fabricateWigTrackline(track);
			}
		    }

		// is it weird that the loader customFactoryTestExistence() did not do this for me?
		char *wibFilePath = hashFindVal(track->tdb->settingsHash, "wibFile");
		if (wibFilePath && fileExists(wibFilePath))
		    {
		    track->wibFile = wibFilePath;
		    }
		char *mafFilePath = hashFindVal(track->tdb->settingsHash, "mafFile");
		if (mafFilePath && fileExists(mafFilePath))
		    {
		    track->wibFile = mafFilePath;
		    }
		// old vcf type (not vcfTabix)
		char *vcfFilePath = hashFindVal(track->tdb->settingsHash, "vcfFile");
		if (vcfFilePath && fileExists(vcfFilePath))
		    {
		    track->wibFile = vcfFilePath;
		    }

		if (track->dbTrack && track->dbDataLoad && track->dbTableName)
		    {
		    extra->tableRows = sqlTableSizeIfExists(ctConn, track->dbTableName); 
		    extra->tableDataLength = sqlTableDataSizeFromSchema(ctConn, CUSTOM_TRASH, track->dbTableName);
		    }


		char *bigDataUrl = hashFindVal(track->tdb->settingsHash, "bigDataUrl");
		if (bigDataUrl)
		    extra->bigDataUrl = bigDataUrl;

		slAddHead(&result->ctExtras,extra);
		}
	    slReverse(&result->ctExtras);

	    }

	slAddHead(&resultList,result);
	}
    else if (sameString("trackHubs", namePt))
	{
	cgiDecode(dataPt, dataPt, strlen(dataPt));
	if (pTrackHubsVar)
	    *pTrackHubsVar = cloneString(dataPt);
	}

    namePt = nextNamePt;
    }
hFreeConn(&ctConn);
freeMem(contentsToChop);

slReverse(&resultList);
return resultList;

}

void processCtsForDownload(char *contents)
/* Process the saved session cart contents,
 * looking for custom-tracks which are db-specific. */
{

struct downloadResults *resultList = processCtsForDownloadInternals(contents, NULL);

printf("<H2>Custom Tracks</H2>");

printf("You can backup the custom tracks that were previously uploaded to the UCSC Genome Browser servers.<br>"
    "<b>Note:</b> Only plain-text custom tracks uploaded to UCSC will be downloaded. "
    "The big* data formats custom tracks are not downloaded due to the data hosted externally.<BR><BR>\n"
    "The download will create a single .tar.gz file with custom track data, "
    "which you can then download to your own machine for use as a backup.<br>"
    "\n");

struct downloadResults *result = NULL;


int ctCount = 0;
long totalDataToDownload = 0;

for (result=resultList; result; result=result->next)
    {

    if (!result->ctPath)  // ct.bed was missing for the db
	continue;

    printf("<h3>Database %s</h3>\n", result->db);

    if (startsWith("hub_", result->db))
	{
	unsigned hubId = isLiveHub(result->db, NULL, TRUE);
	if (hubId == 0)
	    {
	    printf("Skipping HUB Database %s. "
		"This hub db is not currently available and will not get backed up.<BR><BR>\n", result->db);
	    continue; // skip db if not active.
	    }
	}
    else
	{
	if (!isActiveDb(result->db))
	    {
	    printf("Skipping Database %s. "
		"The db is not currently active and will not get backed up.<BR><BR>\n", result->db);
	    continue; // skip db if not active.
	    }
	}

    struct customTrack *cts = result->cts, *track = NULL;
    struct ctExtra *extras = result->ctExtras, *extra = NULL;

    printf("<table>\n");

    for (track=cts,extra=extras; track; track=track->next,extra=extra->next)
	{

	boolean wibMissing = track->wibFile && !fileExists(track->wibFile);

	if (extra->bigDataUrl || (track->dbTrack && track->dbDataLoad && track->dbTableName && !wibMissing))
	    {
	    long trackDataToDownload = 0;

	    ++ctCount;

	    if (!extra->bigDataUrl)
		{
		char greek[32];
		sprintWithGreekByte(greek, sizeof(greek), extra->tableDataLength);
		trackDataToDownload += extra->tableDataLength;

		// handle wiggle cts which have an additional wig binary
		// wibFile=../trash/ct/hgtct_genome_542_dc1750.wib
		// wibFile=../trash/ct/ct_hgwdev_galt_e83f_3892d0.maf 
		// wibFIle=../trash/ct/ct_hgwdev_galt_4ba3_415cc0.vcf
		if (track->wibFile && fileExists(track->wibFile))
		    {
		    long wibFileSize = fileSize(track->wibFile);
		    char greek[32];
		    sprintWithGreekByte(greek, sizeof(greek), wibFileSize);
		    trackDataToDownload += wibFileSize;
		    }
		}

	    // htmlFile=../trash/ct/ct_hgwdev_galt_d011_383c50.html 
	    if (track->htmlFile && fileExists(track->htmlFile))
		{
		long htmlFileSize = fileSize(track->htmlFile);
		char greek[32];
		sprintWithGreekByte(greek, sizeof(greek), htmlFileSize);
		trackDataToDownload += htmlFileSize;
		}

	    char greek[32];
	    sprintWithGreekByte(greek, sizeof(greek), trackDataToDownload);
	    if (trackDataToDownload == 0)  // suppress 0.0 B
		greek[0] = 0; // empty string
	    printf("<tr><td>%s</td><td>%s</td></tr>\n", extra->name, greek); // track name

	    totalDataToDownload += trackDataToDownload;
	    }
	}

    printf("</table>\n");

    }

printf("<br>\n");
printf("%d custom tracks found.<br>\n", ctCount);

char greek[32];
sprintWithGreekByte(greek, sizeof(greek), totalDataToDownload);
printf("Total custom track data to backup: %s ", greek);

if (ctCount > 0)
    {
    cgiMakeButton(hgsMakeDownloadPrefix, "Create custom tracks backup archive");
    }

printf("<br>\n");
printf("<br>\n");

}


void showDownloadSessionCtData(struct hashEl *downloadList)
/* Show download page for the given session */
{
char query[512];
char **row = NULL;
struct sqlResult *sr = NULL;

puts("Content-Type:text/html\n");
cartWebStart(cart, NULL, "Backup Custom Tracks");
jsInit();

struct sqlConnection *conn = hConnectCentral();

printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=POST "
       "ENCTYPE=\"multipart/form-data\">\n",
       hgSessionName());
cartSaveSession(cart);


sqlSafef(query, sizeof(query), "SELECT firstUse, contents from %s "
    "WHERE id=%u",
    "sessionDb", cartSessionRawId(cart));

sr = sqlGetResult(conn, query);

if ((row = sqlNextRow(sr)) != NULL)
    {
    char *db = NULL;
    char *firstUse = row[0];

    printf("<div style=\"max-width:1024px\">");
    printf("<table id=\"sessionTable\" class=\"sessionTable stripe hover row-border compact\" borderwidth=0>\n");
    printf("<thead><tr>"
	   "<TH><B>created on</B></TH>"
	   "<TH><B>assembly</B></TH>"
	   "</tr></thead>");
    printf("<tbody>\n");

    printf("<TR>");

    printf("<TD><nobr>%s</nobr>&nbsp;&nbsp;</TD>", firstUse);

    char *contents = row[1];

    char *dbIdx = NULL;
    if (startsWith("db=", contents))  // the first variable in the cart (rare case)
	dbIdx = contents+3;
    else
	dbIdx = strstr(contents, "&db=") + 4;
	
    if (dbIdx)
	{
	char *dbEnd = strchr(dbIdx, '&');
	if (dbEnd != NULL)
	    db = cloneStringZ(dbIdx, dbEnd-dbIdx);
	else
	    db = cloneString(dbIdx);
	}
    else
	db = cloneString("n/a");
    printf("<TD align=center>%s</td>", db);

    printf("</tr>");

    printf("</tbody>\n");

    printf("</TABLE>\n");

    printf("</div>\n");
    printf("<P></P>\n");

    processCtsForDownload(contents);

    }
sqlFreeResult(&sr);

hDisconnectCentral(&conn);
printf("</FORM>\n");

cartWebEnd();

}

void makeTrashFileLink(char *trashFile, char *cwd, char *outDbDir, char *name)
/* Touch file and make a symlink to it as db/name.ext */
{
// touch to refresh access time so it will not be cleaned out
// this may be redundant in the future but also harmless
readAndIgnore(trashFile);

// make a symlink with name.ext using track name and wibFile extension
// that points to the original wibFile
char newPath[1024];
char toDir[PATH_LEN], toFile[FILENAME_LEN], toExt[FILEEXT_LEN];
splitPath(trashFile, toDir, toFile, toExt);

// record the original path in a file.
safef(newPath, sizeof newPath, "%s/%s%s", outDbDir, name, toExt);

char *oldPath = expandRelativePath(cwd, trashFile);

if (symlink(oldPath, newPath))
    errAbort("failed to symlink from %s to %s", oldPath, newPath);
}
			
void saveSqlDataForTable(char *tableName, FILE *f)
/* append table data as tab-separated text */
{
struct sqlConnection *conn = sqlConnect(CUSTOM_TRASH);
char query[256];
struct sqlResult *sr;
char **row = NULL;

sqlSafef(query, sizeof(query), "select * from %s", tableName);
sr = sqlGetResult(conn, query);
int numFields = sqlCountColumns(sr);
boolean hasBin = (sqlFieldColumn(sr, "bin") != -1);  
struct dyString *dy = dyStringNew(1024);
while ((row=sqlNextRow(sr)))
    {
    int start = 0;
    if (hasBin)    // skip bin column
	++start;
    int i;
    for (i=start; i<numFields; ++i)
	{
	if (i > start)
	    dyStringAppendC(dy, '\t');
	dyStringAppend(dy, row[i]);
	}
    dyStringAppendC(dy, '\n');

    mustWrite(f, dy->string, dy->stringSize);

    dyStringClear(dy);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

void updateProgessFile(char *backgroundProgress, struct dyString *dy)
/* update the progress file as a separate channel */
{
// write to new file
char newName[1024];
safef(newName, sizeof newName, "%s.new", backgroundProgress);
FILE *f = mustOpen(newName, "w");
mustWrite(f, dy->string, dy->stringSize);
carefulClose(&f);
// replace original
if (rename(newName, backgroundProgress))
    errnoAbort("failed to rename progress file");
}

void appendTrashFileToCt(FILE *fct, char *fileName)
/* append wibFile to open fct file */
{
FILE *f = mustOpen(fileName, "r");
long remaining = fileSize(fileName);
int bufSize = 65536;
char *buf = needMem(bufSize);
while (remaining)
    {
    int bufRemain = bufSize;
    if (bufRemain > remaining)
	bufRemain = remaining;
    mustRead(f, buf, bufRemain); // mustRead OK since we are reading from disk
    mustWrite(fct, buf, bufRemain);
    remaining -= bufRemain;
    }
carefulClose(&f);
}

void doOutWigData(char *table, char *database, char *outFile) // should be CUSTOM_TRASH
/* Return wiggle data in variableStep format. */
{
/* Write out wig data in region.  */

struct wiggleDataStream *wds = NULL;
int operations = wigFetchAscii;

operations = wigFetchAscii;

wds = wiggleDataStreamNew(); 

wds->getData(wds, database, table, operations);

wds->asciiOut(wds, database, outFile, TRUE, FALSE); 

wiggleDataStreamFree(&wds);

}

void makeDownloadSessionCtData(char *param1, char *backgroundProgress)
/* Download tables and data to save save in compressed archive. */
{
char query[512];
char **row = NULL;
struct sqlResult *sr = NULL;


// Initialize .progress channel file
struct dyString *dyProg = dyStringNew(256);
dyStringPrintf(dyProg, "please wait, dumping data to archive ...<br>\n");
updateProgessFile(backgroundProgress, dyProg);
lazarusLives(20 * 60);

htmlOpen("Preparing Custom Tracks Backup Archive for Download");
jsInit();

struct sqlConnection *conn = hConnectCentral();

printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=POST "
       "ENCTYPE=\"multipart/form-data\">\n",
       hgSessionName());
cartSaveSession(cart);

sqlSafef(query, sizeof(query), "SELECT contents from %s "
    "WHERE id=%u",
    "sessionDb", cartSessionRawId(cart));

sr = sqlGetResult(conn, query);

// setup temp output dir:
// ../trash/
//  ctBackup/
//   random@#$%/  # random value so it is unique

/* make a directory, including parent directories */

char tempOut[1024];
safef(tempOut, sizeof tempOut, "../trash/ctBackup");
if (!fileExists(tempOut))
    {
    makeDir(tempOut);
    }

char tempOutRand[1024];
// make randomDir 
char *randPath=NULL;
int randCount = 0;
while(TRUE)
    {
    randPath=makeRandomKey(128+33); // at least 128 bits of protection, 33 for the world population size.
    // Avoid the possibility of somehow doing more than one instance of the session
    // backup at the same time.  
    // On the extremely rare chance that the directory already exists,
    // just call the function again.
    safef(tempOutRand, sizeof tempOutRand, "%s/%s", tempOut, randPath);
    if (makeDir(tempOutRand))
	break;
    ++randCount;
    if (randCount > 100) // should never happen
	errAbort("unable to create random output dir.");  
    }

int ctCount = 0;
int foundCount = 0;
if ((row = sqlNextRow(sr)) != NULL)
    {
    ++foundCount;

    char *contents = row[0];


    char *trackHubsVar = NULL;

    struct downloadResults *resultList = processCtsForDownloadInternals(contents, &trackHubsVar);

    struct downloadResults *result = NULL;

    for (result=resultList; result; result=result->next)
	{
	struct customTrack *cts = result->cts, *track = NULL;
	struct ctExtra *extras = result->ctExtras, *extra = NULL;

	// empty db dir can trigger cleaning out of cart ct var for db
        // if it has no ct.bed
	char outDbDir[1024];
	safef(outDbDir, sizeof outDbDir, "%s/%s", tempOutRand, result->db);
	if (!fileExists(outDbDir))
	    {
	    makeDir(outDbDir);
	    }

	if (!result->ctPath)  // ct.bed was missing for the db
	    continue;

	char *hubUrl = NULL;

	if (startsWith("hub_", result->db))
	    {
	    unsigned hubId = isLiveHub(result->db, &hubUrl, FALSE);
	    if (hubId == 0)
		{
		continue; // skip hub if not active.
		}
	    }
	else
	    {
	    if (!isActiveDb(result->db))
		{
		continue; // skip db if not active.
		}
	    }


	printf("<h3>%s</h3>\n", result->db);
	dyStringPrintf(dyProg,"<h3>%s</h3>\n", result->db); 
	updateProgessFile(backgroundProgress, dyProg);
	lazarusLives(20 * 60);

	
	if (startsWith("hub_", result->db))
	    {
	    // Save the hubUrl now in case it gets restored on another machine
	    // that does NOT have that hub loaded on it.
	    // save output as hubUrl text file 
	    char hubUrlPath[1024];
	    safef(hubUrlPath, sizeof hubUrlPath, "%s/hubUrl", outDbDir);
	    FILE *f = mustOpen(hubUrlPath, "w");
	    fprintf(f, "%s", hubUrl);  // NO NEWLINE
	    carefulClose(&f);
	    }

	char cwd[PATH_LEN];
	if (!getcwd(cwd, sizeof(cwd)))
	    errnoAbort("unable to get current dir.");		

	for (track=cts,extra=extras; track; track=track->next,extra=extra->next)
	    {

	    boolean wibMissing = track->wibFile && !fileExists(track->wibFile);

	    if (extra->bigDataUrl || (track->dbTrack && track->dbDataLoad && track->dbTableName && !wibMissing))
		{

		++ctCount;
		printf("%s <br>\n", extra->name);

		dyStringPrintf(dyProg, "%s <br>\n", extra->name);
		updateProgessFile(backgroundProgress, dyProg);
		lazarusLives(20 * 60);


		char outNameCt[2014];
		safef(outNameCt, sizeof outNameCt, "%s/%s.ct", outDbDir, extra->name);
		FILE *fct = mustOpen(outNameCt, "w");

		// write the track header
		if (extra->browserLines)
		    fprintf(fct, "%s", extra->browserLines);  // should have ; converted \n already

		if (!extra->trackLine)
		    errAbort("origTrackLine is NULL!");
		fprintf(fct, "%s\n", extra->trackLine);
    

		if (!extra->bigDataUrl)
		    {

		    // handle wiggle cts which have an additional wig binary
		    // wibFile='../trash/ct/hgtct_genome_542_dc1750.wib'
		    // wibFile='../trash/ct/ct_hgwdev_galt_e83f_3892d0.maf'
		    // wibFile='../trash/ct/ct_hgwdev_galt_4ba3_415cc0.vcf'
		    // symlink for speed, file should not change
		    if (track->wibFile)
			{
			if (endsWith(track->wibFile, ".wib"))
			    {
			    // dump wig as ascii
			    char outNameWig[2014];
			    safef(outNameWig, sizeof outNameWig, "%s/%s.wig", outDbDir, extra->name);
			    doOutWigData(track->dbTableName, CUSTOM_TRASH, outNameWig); // no easy way to append it to .ct directly 
			    // append text to ct
			    appendTrashFileToCt(fct, outNameWig);
			    remove(outNameWig);
			    }
			if (endsWith(track->wibFile, ".maf"))
			    {
			    // append text to ct
			    appendTrashFileToCt(fct, track->wibFile);
			    }
			if (endsWith(track->wibFile, ".vcf"))
			    {
			    // append text to ct
			    appendTrashFileToCt(fct, track->wibFile);
			    }
			}
		    else  // simple BED-like
			{
			saveSqlDataForTable(track->dbTableName, fct);
			}

		    }
		// handle extra htmlFile track doc
		// htmlFile=../trash/ct/ct_hgwdev_galt_d011_383c50.html 
		// symlink for speed, file should not change
		if (track->htmlFile && fileExists(track->htmlFile))
		    {
		    makeTrashFileLink(track->htmlFile, cwd, outDbDir, extra->name);
		    }

		carefulClose(&fct);
		}

	    }
	}

    }
sqlFreeResult(&sr);
 
if (foundCount == 0)
    errAbort("No session found for hgsid=%u", cartSessionRawId(cart));

if (ctCount == 0)
    errAbort("No custom tracks found for hgsid=%u", cartSessionRawId(cart));

char archiveName[1024];
safef(archiveName, sizeof archiveName, "savedSessionCtRaw.tar.gz");

dyStringPrintf(dyProg, "<br>\n");
int saveDySize = dyProg->stringSize;
dyStringPrintf(dyProg, "creating and compressing archive %s <br>\n", archiveName);
updateProgessFile(backgroundProgress, dyProg);
lazarusLives(20 * 60);

// create the archive
char *cwd = cloneString(getCurrentDir());
setCurrentDir(tempOutRand);
char excludeBuf[4096];
safef(excludeBuf, sizeof excludeBuf, "--exclude=%s", archiveName);
char *pipeCmd1[] = { "tar", "-zcphf", archiveName, ".", excludeBuf, NULL};
struct pipeline *pl = pipelineOpen1(pipeCmd1, pipelineWrite | pipelineNoAbort, "/dev/null", NULL, 0);
int sysVal = pipelineWait(pl);
setCurrentDir(cwd);

if (!((sysVal == 0) || (sysVal == 1)))  // we tolerate 1 because tar doesn't like us creating the archive in the directory we're backing up
    errAbort("System call returned %d for:\n  %s", sysVal, pipelineDesc(pl));

dyProg->stringSize = saveDySize;  // restore prev size, popping.
dyProg->string[dyProg->stringSize] = 0;
dyStringPrintf(dyProg, "archive %s created <br>\n", archiveName);
updateProgessFile(backgroundProgress, dyProg);
lazarusLives(20 * 60);

char downPath[1024];
safef(downPath, sizeof downPath, "%s/%s", tempOutRand, archiveName);

printf("<br>\n");

char *encDownPath = cgiEncodeFull(downPath);
char downButName[1024];
safef(downButName, sizeof(downButName), "%s%s", hgsDoDownloadPrefix, encDownPath);

// using getElementById('%s') here is a work-around for special charachters in id.
// I tried escaping, but it did not seem to work with our jQuery version.
// mainly the special characters it sees are periods and slashes.
char downButNameHack[1024];
safef(downButNameHack, sizeof(downButNameHack), "document.getElementById('%s')", downButName);

printf("Download archive as local file:\n");
cgiMakeOnKeypressTextVar(hgsSaveLocalBackupFileName,  
			 cartUsualString(cart, hgsSaveLocalBackupFileName, ""),
			     20, jsPressOnEnter(downButNameHack));
printf(".tar.gz");

char greek[32];
sprintWithGreekByte(greek, sizeof(greek), fileSize(downPath));
printf(" %s ", greek);

printf("<input type='submit' name='%s' id='%s' value='download backup archive'>", downButName, downButName);

// check the file name, make sure it is not blank
char js[1024];
safef(js, sizeof js,
"var textVar = document.getElementById('%s');"
"if (textVar.value != '')"
"    return true;"
"alert('Please enter a filename for the backup archive');"
"return false;"
, hgsSaveLocalBackupFileName);

jsOnEventById("click", downButName, js);

// Add a button that just returns to the saved sessions list.
// because after pressing download and it will go into the users downloads folder,
// the browser remains sitting on this page and needs a way to return
// to the saved sessions list.  Also clear the download name field.
printf(" &nbsp; ");
printf("<input type='submit' id='hgsReturn' value='return'>");
safef(js, sizeof js,
"var textVar = document.getElementById('%s');"
"textVar.value = '';"
"return true;"
, hgsSaveLocalBackupFileName);

jsOnEventById("click", "hgsReturn", js);

printf("<br>\n");
printf("<br>\n");

hDisconnectCentral(&conn);
printf("</FORM>\n");

htmlClose();
fflush(stdout);

}

void doDownloadSessionCtData(struct hashEl *downloadPathList)
/* Download given table to browser to save. */
{

struct hashEl *hel = NULL;

// I think it should only get one at a time, not a list.
hel = downloadPathList;

char *encDownPath = hel->name + strlen(hgsDoDownloadPrefix);
char *downPath = cgiDecodeClone(encDownPath);

char *fileName = cartString(cart, hgsSaveLocalBackupFileName);

char outFile[1024];
safef(outFile, sizeof outFile, "%s.tar.gz", fileName);

long fSize = fileSize(downPath);

printf("Content-Type: application/octet-stream\n");
printf("Content-Disposition: attachment; filename=\"%s\"\n", outFile);
printf("Content-Length: %ld\n", fSize);
printf("\n");

FILE *f = mustOpen(downPath, "r");
long remaining = fSize;
int bufSize = 65536;
char *buf = needMem(bufSize);
while (remaining)
    {
    int bufRemain = bufSize;
    if (bufRemain > remaining)
	bufRemain = remaining;
    mustRead(f, buf, bufRemain); // mustRead OK since we are reading from disk
    mustWrite(stdout, buf, bufRemain);
    remaining -= bufRemain;
    lazarusLives(20 * 60);   // extend keep-alive time. for big downloads on slow connections.
    }
carefulClose(&f);

cartRemove(cart, hgsSaveLocalBackupFileName);

}

