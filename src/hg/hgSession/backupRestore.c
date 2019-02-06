/* backupRestore - Routines related to backing up and restoring the current cart and its custom tracks. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


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


void getBackgroundStatus(char *url)
/* fetch status as the latest complete html block available.
 * fetch progress info instead if background proc still running. */
{
char *html = NULL;
if (fileSize(url)==0)
    {
    htmlOpen("Background Status");
    errAbort("No output found. Expecting output in [%s].", url);
    htmlClose();
    return;
    }

// get the pid and see if it is still running
char pidName[1024];
safef(pidName, sizeof pidName, "%s.pid", url);
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
    //printf("%s", textErr); /* DEBUG REMOVE? more annoying than helpful in some places. */
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
    };

struct downloadResults
    {
    struct downloadResults *next;
    char *db;
    char *ctPath;    // path to ct file in trash
    struct customTrack *cts;  // tracks
    struct ctExtra *ctExtras; // parallel additional info to the tracks
    };

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

		// is it weird that the loader customFactoryTestExistence() did not do this for me?
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

printf("You can backup the custom tracks which you previously uploaded to UCSC Genome Browser servers.<br>"
    "It will create a single .tar.gz file with all session settings and custom track data,<br>"
    "which you can then download to your own machine for use as a backup.<br>"
    "\n");

struct downloadResults *result = NULL;


int nonBigDataUrlCount = 0;
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
	long trackDataToDownload = 0;

	++ctCount;

	// handle user-friendly sizes GB MB KB and B
	if (track->dbTrack && track->dbDataLoad && track->dbTableName)
	    {
	    char greek[32];
	    ++nonBigDataUrlCount;
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

    printf("</table>\n");

    }

printf("<br>\n");
printf("%d custom tracks found.<br>\n", ctCount);
printf("%d custom tracks stored at UCSC.<br>\n", nonBigDataUrlCount);

char greek[32];
sprintWithGreekByte(greek, sizeof(greek), totalDataToDownload);
printf("Total custom track data to backup: %s ", greek);

cgiMakeButton(hgsMakeDownloadPrefix, "create full backup archive");

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
cartWebStart(cart, NULL, "Backup Session Settings including Custom Tracks");
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

    printf("<TD><nobr>%s<nobr>&nbsp;&nbsp;</TD>", firstUse);

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

void makeTrashFileLinkOrCopy(char *trashFile, char *cwd, char *outDbDir, char *name, boolean isCopy)
/* Touch file and make a symlink or copy db/name.ext
 * Also save the original path in a file. */
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

if (isCopy)
    {
    copyFile(oldPath, newPath);
    touchFileFromFile(oldPath, newPath);  // preserve time on copy
    chmod(newPath, 0644);                 // copyFile makes 0777
    }
else
    {
    if (symlink(oldPath, newPath))
	errAbort("failed to symlink from %s to %s", oldPath, newPath);
    }
}
			
void saveSqlCreateForTable(char *tableName, char *outDbDir, char *name)
/* save create table statement as name.sql in outDbDir */
{
struct sqlConnection *conn = sqlConnect(CUSTOM_TRASH);
char *sql = sqlGetCreateTable(conn, tableName);
char outName[2014];
safef(outName, sizeof outName, "%s/%s.sql", outDbDir, name);
FILE *f = mustOpen(outName, "w");
mustWrite(f, sql, strlen(sql));
carefulClose(&f);
sqlDisconnect(&conn);
}

void saveSqlDataForTable(char *tableName, char *outDbDir, char *name)
/* save table data as tab-separated name.txt in outDbDir */
{
struct sqlConnection *conn = sqlConnect(CUSTOM_TRASH);
char query[256];
struct sqlResult *sr;
char **row = NULL;

char outName[2014];
safef(outName, sizeof outName, "%s/%s.txt", outDbDir, name);
FILE *f = mustOpen(outName, "w");
sqlSafef(query, sizeof(query), "select * from %s", tableName);
sr = sqlGetResult(conn, query);
int numFields = sqlCountColumns(sr);
struct dyString *dy = dyStringNew(1024);
while ((row=sqlNextRow(sr)))
    {
    int i;
    for (i=0; i<numFields; ++i)
	{
	if (i > 0)
	    dyStringAppendC(dy, '\t');
	dyStringAppend(dy, row[i]);
	}
    dyStringAppendC(dy, '\n');
    mustWrite(f, dy->string, dy->stringSize);
    dyStringClear(dy);
    }
sqlFreeResult(&sr);
carefulClose(&f);
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


void makeDownloadSessionCtData(char *param1, char *backgroundProgress)
/* Download tables and data to save save in compressed archive. */
{
char query[512];
char **row = NULL;
struct sqlResult *sr = NULL;


// Initialize .progress channel file
struct dyString *dyProg = newDyString(256);
dyStringPrintf(dyProg, "please wait, dumping data to archive ...<br>\n");
updateProgessFile(backgroundProgress, dyProg);
lazarusLives(20 * 60);

htmlOpen("Preparing Settings and Custom Tracks Backup Archive for Download");
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

int foundCount = 0;
if ((row = sqlNextRow(sr)) != NULL)
    {
    ++foundCount;

    char *contents = row[0];

    char outFile[1024];
    safef(outFile, sizeof outFile, "%s/cart", tempOutRand);
    FILE *f = mustOpen(outFile, "w");	
    mustWrite(f, contents, strlen(contents));	
    carefulClose(&f);	

    char *trackHubsVar = NULL;

    struct downloadResults *resultList = processCtsForDownloadInternals(contents, &trackHubsVar);

    // Save original hub id and url for each in trackHubs var.
    // This is needed when restoring the cart onto a foreign system such as a different mirror server.
    safef(outFile, sizeof outFile, "%s/hubList", tempOutRand);
    f = mustOpen(outFile, "w");
    if (trackHubsVar)
	{
	// parse into hub ids
	char *list = cloneString(trackHubsVar);
	char **hubStr = NULL;
	int hubCount = chopByChar(list, ' ', NULL, 0);
	AllocArray(hubStr, hubCount + 1);
	chopByChar(list, ' ', hubStr, hubCount);
	int i;
	// save each id with its URL to hubList file in archive root.
	for (i=0; i<hubCount; ++i)
	    {
	    unsigned hubId = sqlUnsigned(hubStr[i]);
	    char *hubUrl = getHubUrlFromId(hubId);
	    if (hubUrl)
		fprintf(f, "%d %s", hubId, hubUrl);
	    }
	}
    carefulClose(&f);	

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

	// Get the ct.bed file in trash as a real file.
	// These are small, and a copy (instead of symlink)
	// goes quickly and prevents it from being modified?
	// HMM if hgCustom is truly copy-on-write, then symlink would be ok.
	// with TRUE it makes a copy
	// ctPath should exist and is required.
	makeTrashFileLinkOrCopy(result->ctPath, cwd, outDbDir, "ct", TRUE);


	for (track=cts,extra=extras; track; track=track->next,extra=extra->next)
	    {

	    printf("%s <br>\n", extra->name);

	    dyStringPrintf(dyProg, "%s <br>\n", extra->name);
	    updateProgessFile(backgroundProgress, dyProg);
	    lazarusLives(20 * 60);
	    
	    if (track->dbTrack && track->dbDataLoad && track->dbTableName)
		{
		saveSqlCreateForTable(track->dbTableName, outDbDir, extra->name);
		saveSqlDataForTable(track->dbTableName, outDbDir, extra->name);

		// handle wiggle cts which have an additional wig binary
		// wibFile='../trash/ct/hgtct_genome_542_dc1750.wib'
		// wibFile='../trash/ct/ct_hgwdev_galt_e83f_3892d0.maf'
		// wibFile='../trash/ct/ct_hgwdev_galt_4ba3_415cc0.vcf'
		// symlink for speed, file should not change
		if (track->wibFile && fileExists(track->wibFile))
		    {
		    makeTrashFileLinkOrCopy(track->wibFile, cwd, outDbDir, extra->name, FALSE);
		    }
		}

	    // handle extra htmlFile track doc
	    // htmlFile=../trash/ct/ct_hgwdev_galt_d011_383c50.html 
	    // symlink for speed, file should not change
	    if (track->htmlFile && fileExists(track->htmlFile))
		{
		makeTrashFileLinkOrCopy(track->htmlFile, cwd, outDbDir, extra->name, FALSE);
		}

	    }
	}

    }
sqlFreeResult(&sr);
 
if (foundCount == 0)
    errAbort("No session found for hgsid=%u", cartSessionRawId(cart));

// create the version file
char versionPath[1024];
safef(versionPath, sizeof versionPath, "%s/%s", tempOutRand, UCSC_GB_BACKUP_VERSION_FILENAME);
FILE *f = mustOpen(versionPath, "w");
fprintf(f, "Version 1.0\n");
carefulClose(&f);

char archiveName[1024];
safef(archiveName, sizeof archiveName, "savedSessionCtRaw.tar.gz");

dyStringPrintf(dyProg, "<br>\n");
int saveDySize = dyProg->stringSize;
dyStringPrintf(dyProg, "creating and compressing archive %s <br>\n", archiveName);
updateProgessFile(backgroundProgress, dyProg);
lazarusLives(20 * 60);

// create the archive
char cmd[2048];
safef(cmd, sizeof cmd, "cd %s; tar -cpzhf %s *", tempOutRand, archiveName);
mustSystem(cmd);

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

printf("<input type='submit' name='%s' id='%s' value='download full backup archive'>", downButName, downButName);

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
// to the saved sessions list.
printf(" &nbsp; ");
printf("<input type='submit' value='return'>");

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

}

long showFileIfExists(char *dbDir, char *track, char *ext)
/* Show file if exists and return size */
{
char path[1024];
safef(path, sizeof path, "%s/%s%s", dbDir, track, ext); 
long size = fileSize(path);
if (size < 0) // file not exists returns -1
    size = 0;
return size;
}

boolean isFullLastLine(char *text, char terminator)
/* see if text ends in the terminator,
 * indicating a fully-terminated line */
{
boolean result = FALSE;
long len = strlen(text);
if (len > 0 && text[len-1] == terminator)
    result = TRUE;
return result;
}

char *findValueInCtLine(char *line, char *varName)
/* find the value of the given variable name or abort */
{
char pat[1024];
safef(pat, sizeof pat, "\t%s='", varName);
char *s = strstr(line, pat);
if (!s)
    errAbort("%s not found in findValueInCtLine", varName);
s += strlen(pat);
char *e = strchr(s, '\'');
if (!e)
    errAbort("terminating single quote not found in findValueInCtLine");
return cloneStringZ(s, e-s);
}


long showDb(char *dbDir)
/* Show db dir. Return total size. */
{
long totalSize = 0;
// read from ct bed file
char ctPath[1024];
safef(ctPath, sizeof ctPath, "%s/ct.bed", dbDir); 
char *ctText = NULL;
readInGulp(ctPath, &ctText, NULL);
// parse into lines
boolean fullLastLine = isFullLastLine(ctText, '\n');  // test before chopping
char **lines = NULL;
int lineCount = chopByChar(ctText, '\n', NULL, 0);
AllocArray(lines, lineCount);
chopByChar(ctText, '\n', lines, lineCount);
if (fullLastLine) // chopByChar count needs to reduce by 1 if line ends in the chopByChar
    --lineCount;

printf("<table>");
int lineNum;
for(lineNum=0; lineNum<lineCount; ++lineNum)
    {
    long trackSize = 0;
    char *track = findValueInCtLine(lines[lineNum], "name");
    
    trackSize += showFileIfExists(dbDir, track, ".txt");
    trackSize += showFileIfExists(dbDir, track, ".html");
    trackSize += showFileIfExists(dbDir, track, ".wib");
    trackSize += showFileIfExists(dbDir, track, ".maf");
    trackSize += showFileIfExists(dbDir, track, ".vcf");

    char greek[32];
    sprintWithGreekByte(greek, sizeof(greek), trackSize);
    if (trackSize == 0)
	greek[0] = 0;   // do not show size 0

    printf("<tr><td>%s</td><td>%s</td></tr>\n", track, greek);

    totalSize += trackSize;
    }
printf("</table>");
return totalSize;
}

char *getNewHub(char *dbDir, char *db, boolean talkative)
/* find or add url */
{

// Note: hubStatus is the real main table. hubConnect only has 7 rows and does not exist on RR.

// get the hubUrl which hopefully got saved during backup.
char hubUrlPath[1024];
safef(hubUrlPath, sizeof hubUrlPath, "%s/hubUrl", dbDir);
char *hubUrl = NULL;
if (!fileExists(hubUrlPath))
    errAbort("missing %s/hubUrl file in archive", dbDir);
readInGulp(hubUrlPath, &hubUrl, NULL);

char *errMsg = NULL;
unsigned newHubId = hubFindOrAddUrlInStatusTable(NULL, NULL, hubUrl, &errMsg);
/* find this url in the status table, and return its id and errorMessage (if an errorMessage exists) */
if (errMsg)
    printf("HUB error message=%s<BR>\n", errMsg);
if (newHubId == 0)
    {
    if (talkative)
	{
	printf(" (hub %s NOT FOUND! Its custom tracks will be NOT be restored.)<br>\n", hubUrl);
	return NULL;  // will skip hub not active.
	}
    }

char *dbName = trackHubSkipHubName(db);

char newDb[1024];
safef(newDb, sizeof newDb, "hub_%u_%s", newHubId, dbName);

unsigned newHubId2 = isLiveHub(newDb, NULL, talkative);
if (newHubId2 == 0)
    {
    if (talkative)
	{
	printf("Hub database %s NOT FOUND! Its custom tracks will be NOT be restored.<br>\n", newDb);
	}
    return NULL;  // will skip hub db not found.
    }
return cloneString(newDb);
}

long showArchive(char *archDir)
/* Show archive dir. Return total size. */
{
long totalSize = 0;
struct fileInfo *fI = NULL, *fIList = listDirX(archDir, "*", FALSE);
for(fI=fIList; fI; fI=fI->next)
    {
    if (fI->isDir)
	{
	char *db = fI->name;
    	printf("<h3>%s</h3>\n", db);

	char dbDir[1024];
	safef(dbDir, sizeof dbDir, "%s/%s", archDir, db);


	if (startsWith("hub_", db))
	    {
	    char *newDb = getNewHub(dbDir, db, TRUE);
	    if (!newDb)
		{
		continue; // skip if hub not active or db not found.
		}
	    }
	else
	    {
	    if (!isActiveDb(db))
		{
		printf(" Active database %s NOT FOUND! Its custom tracks will be NOT be restored.<br>\n", db);
		continue;
		}
	    }
	
	totalSize += showDb(dbDir);
    	printf("<br>\n");
	}
    }
return totalSize;
}


void extractUploadSessionCtData(
    char *param1, char *param1Value, 
    char *param2, char *param2Value, 
    char *param3, char *param3Value, 
    char *backgroundProgress)
/* Extract uploaded archive to restore cts. */
{

// Initialize .progress channel file
struct dyString *dyProg = newDyString(256);
dyStringPrintf(dyProg, "please wait, ...<br>\n");
updateProgessFile(backgroundProgress, dyProg);
lazarusLives(20 * 60);

htmlOpen("Extracting Archive with Settings and Custom Tracks");

printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=POST "
       "ENCTYPE=\"multipart/form-data\">\n",
       hgSessionName());
cartSaveSession(cart);

// List should have these three
// hgS_extractUpload_hub_9614_Anc11__binary     # because of background exec, it is no longer valid memory pointer
// hgS_extractUpload_hub_9614_Anc11__filename
// hgS_extractUpload_hub_9614_Anc11__filepath   # added for background process
if (!endsWith(param1, "__binary"))
    errAbort("Please choose a saved session custom tracks local backup archive file (.tar.gz) to upload");
char *fileBinaryCoords = param1Value;

printf("<br>\n");

if (!endsWith(param2, "__filename"))
    errAbort("expected cart name to end in __filename");
char *fileName = param2Value;

if (!endsWith(fileName, ".tar.gz"))
    errAbort("Invalid file name %s: Expecting archive with .tar.gz file name extension", fileName);

if (!endsWith(param3, "__filepath"))
    errAbort("expected cart name to end in __filepath");
char *filePath = param3Value;

// setup temp output dir:
// ../trash/
//  ctRestore/
//   random@#$%/  # random value so it is unique

/* make a directory, including parent directories */

char tempOut[1024];
safef(tempOut, sizeof tempOut, "../trash/ctRestore");
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

char *binInfo = cloneString(fileBinaryCoords);
char *words[2];
//char *mem;
chopByWhite(binInfo, words, ArraySize(words));
//mem = (char *)sqlUnsignedLong(words[0]);
// mem offset is invalid because of forked process.
unsigned long size = sqlUnsignedLong(words[1]);

unsigned long fSize = fileSize(filePath);
if (size != fSize)
    errAbort("uploaded binary file %s should be size %lu, found size %lu",
	fileName, size, fSize);

char greekSize[32];
sprintWithGreekByte(greekSize, sizeof(greekSize), size);
printf("Contents of archive <B>%s</B> (%s).<br>\n", fileName, greekSize);

char tempOutFile[1024];
safef(tempOutFile, sizeof tempOutFile, "%s/%s", tempOutRand, fileName);

// move our uploaded binary file where we really want it.
if (rename(filePath, tempOutFile))
    errnoAbort("failed to rename uploaded binary file from %s to %s", filePath, tempOutFile);

dyStringPrintf(dyProg, "decrompressing archive %s\n", fileName);
updateProgessFile(backgroundProgress, dyProg);
lazarusLives(20 * 60);

// create the archive
char cmd[2048];
safef(cmd, sizeof cmd, "cd %s; tar -xpzf %s", tempOutRand, fileName);
mustSystem(cmd);

// check the version file
char versionPath[1024];
safef(versionPath, sizeof versionPath, "%s/%s", tempOutRand, UCSC_GB_BACKUP_VERSION_FILENAME);
if (!fileExists(versionPath))
    {
    errAbort("This is not a valid backup archive. Unable to proceed.");
    }
FILE *f = mustOpen(versionPath, "r");
char version[1024];
mustGetLine(f, version, sizeof version);
carefulClose(&f);
if (!sameString(version, "Version 1.0\n"))
    errAbort("Unrecognized version number in backup archive.");

long totalSize = showArchive(tempOutRand);
printf("<br>\n");

char greek[32];
sprintWithGreekByte(greek, sizeof(greek), totalSize);
printf("Total Size %s ", greek);

char *encRandPath = cgiEncodeFull(randPath);

char buf[1024];
safef(buf, sizeof(buf), "%s%s", hgsDoUploadPrefix, encRandPath);
cgiMakeButton(buf, "restore");
printf(" these settings and custom tracks into the current session<br>\n");

printf("<br>\n");
printf("<br>\n");

printf("</FORM>\n");

htmlClose();
fflush(stdout);

}

struct uploadResults
/* remember track results */
    {
    struct uploadResults *next;
    char *trackName;
    char *origCtLine;
    char *newCtLine;
    };

int findTrackInCtLines(char **lines, int lineCount, char *trackName)
/* search lines for my track or abort if not found */
{
char pat[1024];
safef(pat, sizeof pat, "\tname='%s'\t", trackName);
int i;
for(i=0; i<lineCount; ++i)
    {
    if (strstr(lines[i], pat))
	return i;
    }
errAbort("search failed for track %s in findTrackInLines", trackName);
return -1; // never gets here
}

long uploadFileIfExists(char *dbDir, char *track, char *ext, struct uploadResults *result,
  char *backgroundProgress, struct dyString *dyProg)
/* Show file if exists and return size */
{
char path[1024];
safef(path, sizeof path, "%s/%s%s", dbDir, track, ext); 
long size = fileSize(path);
if (size >= 0) // file exists
    {

    struct tempName tn;
    trashDirFile(&tn, "ct", CT_PREFIX, ext);
    char *newPath = cloneString(tn.forCgi); 

    
    // patch newCtLine with new value 
    char *keyword = NULL;
    if (sameString(ext,".html"))
	keyword = "htmlFile";
    if (sameString(ext,".wib"))
	keyword = "wibFile";
    if (sameString(ext,".maf"))
	keyword = "mafFile";
    if (sameString(ext,".vcf"))
	keyword = "vcfFile";
    
    char *origValue = findValueInCtLine(result->origCtLine, keyword);

    // patch old name to new in newCtLine
    // cannot include trailing tab since last fields has none.
    char oldPat[1024];
    safef(oldPat, sizeof oldPat, "\t%s='%s'", keyword, origValue);
    char newPat[1024];
    safef(newPat, sizeof newPat, "\t%s='%s'", keyword, newPath);
    char *newerCtLine = replaceChars(result->newCtLine, oldPat, newPat);
    if (sameString(newerCtLine, result->newCtLine))
	errAbort("Old name %s not found while swapping in new value for %s in new ct bed line", 
	    origValue, keyword);
    result->newCtLine = newerCtLine;

    if (sameString(ext,".vcf"))  
	{    
	// have to patch the new ct table with newPath
	char *table = findValueInCtLine(result->newCtLine, "dbTableName");
	char sql[1024];
	sqlSafef(sql, sizeof sql, "update %s set filename='%s'", table, newPath);
	struct sqlConnection *ctConn = hAllocConn(CUSTOM_TRASH);
	sqlUpdate(ctConn, sql);
	hFreeConn(&ctConn);
	}

    // mv for speed.
    if (rename(path, newPath))
	errnoAbort("could not rename file %s -> %s\n", path, newPath);
    chmod(newPath, 0644);
    }
else
    {
    // fileSize returns -1 for files not found.
    // caller does not need to know if file not found. but is adding up totals.
    size = 0;  
    }
return size;
}

char *saveCtsForUpload(char *dbDir, struct uploadResults *results)
/* Save new ct.bed from uploadResults.
 * Returns path to the new ct.bed in trash. */
{
struct uploadResults *result = NULL;
char currentCtPath[1024]; // something to merge with
safef(currentCtPath, sizeof currentCtPath, "%s/ct.bed", dbDir);
if (!fileExists(currentCtPath))
    return NULL;  // instead of merging, just drop the ctfile var

// use trash function to create the new output
struct tempName tn;
trashDirFile(&tn, "ct", CT_PREFIX, ".bed");
char *newPath = cloneString(tn.forCgi); 

FILE *f = mustOpen(newPath, "w");
// write 
for(result = results; result; result = result->next)
    {
    fprintf(f, "%s\n", result->newCtLine);
    }

carefulClose(&f);

return newPath;
}

char *mergeNewHubIdIntoTrackHubs(char *currentHubList, unsigned newHubId)
/* if newHubId is not already in the list, add it. */
{
// parse into hub ids
char *list = cloneString(currentHubList);
char **hubStr = NULL;
int hubCount = chopByChar(list, ' ', NULL, 0);
AllocArray(hubStr, hubCount + 1);
chopByChar(list, ' ', hubStr, hubCount);
int i;
for (i=0; i<hubCount; ++i)
    {
    if (newHubId==sqlUnsigned(hubStr[i]))
	return currentHubList;  // already in list, nothing to do
    }
// not found, add it.
int len = strlen(currentHubList) + 20;
char *result = needMem(len);
if (isEmpty(currentHubList))
    safef(result, len, "%u", newHubId);
else
    safef(result, len, "%s %u", currentHubList, newHubId);
return result;
}

void saveContentsToTable(char *contents, unsigned id, char *table)
/* save updated contents back to sessionDb or userDb */
{
struct sqlConnection *conn = hConnectCentral();

// write newContents back to saved session namedSessionDb
int contentLength = strlen(contents);
struct dyString *update = dyStringNew(contentLength*2);
sqlDyStringPrintf(update, "UPDATE %s set contents='", table);
dyStringAppendN(update, contents, contentLength);
dyStringPrintf(update, "', lastUse=now(), useCount=useCount+1 "
	       "where id=%u", id);
sqlUpdate(conn, update->string);
dyStringFree(&update);

hDisconnectCentral(&conn);
}

void processContentsForTrackHubs(char **pContents, char *trackHubs)
/* Process the cart contents, updating hub-related variables */
{

char *contents = *pContents;

char *contentsToChop = cloneString(contents);
char *namePt = contentsToChop;
int contentLength = strlen(contents);
struct dyString *newContents = dyStringNew(contentLength+1);
struct dyString *oneSetting = dyStringNew(contentLength / 4);

// strip hgHubConnect.hub.* and trackHubs vars
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
    dyStringClear(oneSetting);
    dyStringPrintf(oneSetting, "%s=%s%s",
		   namePt, dataPt, (nextNamePt ? "&" : ""));
    if (startsWith("hgHubConnect.hub.", namePt))
	{
	// delete from output.
	}
    else if (sameString(namePt, "trackHubs"))
	{
	// delete from output.
	}
    else
	{
	// copy settings as is
	dyStringAppend(newContents, oneSetting->string);
	}

    namePt = nextNamePt;
    }

// add trackHubs var back into the output.
char *encVal = cgiEncode(trackHubs);
dyStringClear(oneSetting);
if (newContents->stringSize > 0)  // not empty string
    dyStringAppendC(oneSetting, '&');
dyStringPrintf(oneSetting, "%s=%s",
       "trackHubs", encVal);
dyStringAppend(newContents, oneSetting->string);

// add hgHubConnect.hub.* vars back into the output
// parse into hub ids
char *list = cloneString(trackHubs);
char **hubStr = NULL;
int hubCount = chopByChar(list, ' ', NULL, 0);
AllocArray(hubStr, hubCount + 1);
chopByChar(list, ' ', hubStr, hubCount);
int i;
// add for each id 
for (i=0; i<hubCount; ++i)
    {
    unsigned hubId = sqlUnsigned(hubStr[i]);
    dyStringClear(oneSetting);
    if (newContents->stringSize > 0)  // not empty string
	dyStringAppendC(oneSetting, '&');
    dyStringPrintf(oneSetting, "hgHubConnect.hub.%d=1", hubId);
    dyStringAppend(newContents, oneSetting->string);
    }

freez(pContents);
*pContents = cloneString(newContents->string);

dyStringFree(&oneSetting);
dyStringFree(&newContents);
freeMem(contentsToChop);

}

void processCtsForUploadInternals(char **pContents, char *dbDir, char *db, struct uploadResults *results, char *newDb)
/* Process the saved session cart contents,
 * looking for custom-tracks which are db-specific. */
{

/* Parse the CGI-encoded session contents into {var,val} pairs and search
 * for custom tracks.  If found, refresh the custom track.  
 * Parsing code taken from cartParseOverHash.  */

char *contents = *pContents;

char *contentsToChop = cloneString(contents);
char *namePt = contentsToChop;
int contentLength = strlen(contents);
struct dyString *newContents = dyStringNew(contentLength+1);
struct dyString *oneSetting = dyStringNew(contentLength / 4);

boolean found = FALSE;
boolean hubConnectFound = FALSE;
boolean trackHubsLineFound = FALSE;

boolean isHub = FALSE;
// newDb can be NULL on unavailable hub
// we still can clear out the CT_FILE cartvar 
// triggered if ct.bed does not exist in archive dbDir
if (newDb && startsWith("hub_", newDb))
    isHub = TRUE;

unsigned newHubId = 0;
char newHubConnectName[256];
if (isHub)
    {
    newHubId = hubIdFromTrackName(newDb);
    safef(newHubConnectName, sizeof newHubConnectName, "hgHubConnect.hub.%u", newHubId);
    }

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
    dyStringClear(oneSetting);
    dyStringPrintf(oneSetting, "%s=%s%s",
		   namePt, dataPt, (nextNamePt ? "&" : ""));
    if (startsWith(CT_FILE_VAR_PREFIX, namePt))
	{
	char *thisDb = namePt + strlen(CT_FILE_VAR_PREFIX);
	if (sameString(thisDb, db))
	    {
	    found = TRUE;
	    //cgiDecode(dataPt, dataPt, strlen(dataPt));  old way
	    char *newMergedCtPath = saveCtsForUpload(dbDir, results);
	    if (newMergedCtPath)
		{
		// add this back into the output.
		char newName[1024];
		safef(newName, sizeof newName, "%s%s", CT_FILE_VAR_PREFIX, newDb);
		char *encMergedCtPath = cgiEncode(newMergedCtPath);
		dyStringClear(oneSetting);
		dyStringPrintf(oneSetting, "%s=%s%s",
		       newName, encMergedCtPath, (nextNamePt ? "&" : ""));
		dyStringAppend(newContents, oneSetting->string);
		}
	    }
	else
	    {
	    // copy settings as is
	    dyStringAppend(newContents, oneSetting->string);
	    }
	}
    else if (isHub && sameString(namePt, newHubConnectName))
	{
	hubConnectFound = TRUE;
	// add this back into the output.
	dyStringClear(oneSetting);
	dyStringPrintf(oneSetting, "%s=1%s",
	       newHubConnectName, (nextNamePt ? "&" : ""));
	dyStringAppend(newContents, oneSetting->string);
	}
    else if (isHub && sameString(namePt, "trackHubs"))
	{
	trackHubsLineFound = TRUE;
	// add this back into the output.
	cgiDecode(dataPt, dataPt, strlen(dataPt));
	char *newVal = mergeNewHubIdIntoTrackHubs(dataPt, newHubId);
	char *encVal = cgiEncode(newVal);
	dyStringClear(oneSetting);
	dyStringPrintf(oneSetting, "%s=%s%s",
	       "trackHubs", encVal, (nextNamePt ? "&" : ""));
	dyStringAppend(newContents, oneSetting->string);
	}
    else
	{
	// copy settings as is
	dyStringAppend(newContents, oneSetting->string);
	}

    namePt = nextNamePt;
    }
if (!found)
    {
    // add this back into the output.
    char newName[1024];
    safef(newName, sizeof newName, "%s%s", CT_FILE_VAR_PREFIX, newDb);
    char *newMergedCtPath = saveCtsForUpload(dbDir, results);
    if (newMergedCtPath)
	{
	char *encMergedCtPath = cgiEncode(newMergedCtPath);
	dyStringClear(oneSetting);
	if (newContents->stringSize > 0)  // not empty string
	    dyStringAppendC(oneSetting, '&');
	dyStringPrintf(oneSetting, "%s=%s", newName, encMergedCtPath);
	dyStringAppend(newContents, oneSetting->string);
	}
    }
if (isHub && !hubConnectFound)
    {
    // add this back into the output.
    dyStringClear(oneSetting);
    if (newContents->stringSize > 0)  // not empty string
	dyStringAppendC(oneSetting, '&');
    dyStringPrintf(oneSetting, "%s=1", newHubConnectName);
    dyStringAppend(newContents, oneSetting->string);
    }
if (isHub && !trackHubsLineFound)
    {
    // add this back into the output.
    char *newVal = mergeNewHubIdIntoTrackHubs("", newHubId);
    char *encVal = cgiEncode(newVal);
    dyStringClear(oneSetting);
    if (newContents->stringSize > 0)  // not empty string
	dyStringAppendC(oneSetting, '&');
    dyStringPrintf(oneSetting, "%s=%s",
	   "trackHubs", encVal);
    dyStringAppend(newContents, oneSetting->string);
    }

freez(pContents);
*pContents = cloneString(newContents->string);

dyStringFree(&oneSetting);
dyStringFree(&newContents);
freeMem(contentsToChop);

}


long uploadDb(char **pContents, char *dbDir, char *db, char *newDb, char *backgroundProgress, struct dyString *dyProg)
/* Show db dir. Return total size. */
{
long totalSize = 0;
// read from ct bed file
char ctPath[1024];
safef(ctPath, sizeof ctPath, "%s/ct.bed", dbDir); 
char *ctText = NULL;
readInGulp(ctPath, &ctText, NULL);
// parse into lines
boolean fullLastLine = isFullLastLine(ctText, '\n');  // test before chopping
char **lines = NULL;
int lineCount = chopByChar(ctText, '\n', NULL, 0);
AllocArray(lines, lineCount);
chopByChar(ctText, '\n', lines, lineCount);
if (fullLastLine) // chopByChar count needs to reduce by 1 if line ends in the chopByChar
    --lineCount;

struct uploadResults *results = NULL, *result = NULL;
int lineNum;
for(lineNum=0; lineNum<lineCount; ++lineNum)
    {

    AllocVar(result);
    result->origCtLine = cloneString(lines[lineNum]);
    result->newCtLine  = cloneString(lines[lineNum]);

    char *track = findValueInCtLine(lines[lineNum], "name");
    
    printf("&nbsp; %s<br>\n", track);
    result->trackName = cloneString(track);

    // every track should have a .txt file
    char path[1024];
    safef(path, sizeof path, "%s/%s.txt", dbDir, track); 
    if (fileExists(path))
	{

	// create .sql table name
	char prefix[16];
	static int dbTrackCount = 0;
	struct sqlConnection *ctConn = hAllocConn(CUSTOM_TRASH);

	++dbTrackCount;
	safef(prefix, sizeof(prefix), "t%d", dbTrackCount);
	char *table = sqlTempTableName(ctConn, prefix);

	dyStringPrintf(dyProg, "Creating table %s<br>\n", table);
	updateProgessFile(backgroundProgress, dyProg);
	lazarusLives(30 * 60);

	// read from .sql file
	char sqlPath[1024];
	safef(sqlPath, sizeof sqlPath, "%s/%s.sql", dbDir, track); 
	char *sql = NULL;
	readInGulp(sqlPath, &sql, NULL);

	if (!startsWith("CREATE TABLE ", sql))
	    errAbort("Invalid SQL input. for track %s", track);

	// patch old name to new in create sql
	char *origTableName = findValueInCtLine(result->origCtLine, "dbTableName");
	char oldPat[1024];
	safef(oldPat, sizeof oldPat, "CREATE TABLE `%s`", origTableName);
	char newPat[1024];
	safef(newPat, sizeof newPat, "CREATE TABLE `%s`", table);
	char *newSql = replaceChars(sql, oldPat, newPat);
	if (sameString(sql,newSql))
	    errAbort("Old name %s not found while swapping in table name %s in sql for new ct table", 
		origTableName, table);

	char sqlNoInj[1024];
	safef(sqlNoInj, sizeof sqlNoInj, "%s%s", NOSQLINJ, newSql); // hack to get NOSQLINJ tag
	    
	// create table using .sql
	sqlUpdate(ctConn, sqlNoInj);

	// Read the row. split on \t tab. put single quotes around each field, and commas.
	char txtPath[1024];
	safef(txtPath, sizeof txtPath, "%s/%s.txt", dbDir, track); 

	sqlLoadTabFile(ctConn, txtPath, table, 0); 
	// Unable to use SQL_TAB_FILE_ON_SERVER on RR since customTrash is on another server.

	// touch will add it to metaInfo
	ctTouchLastUse(ctConn, table, TRUE); // must use TRUE

	// patch newCtLine with new value for dbTableName.
	// last field has no trailing tab.
	safef(oldPat, sizeof oldPat, "\t%s='%s'", "dbTableName", origTableName);
	safef(newPat, sizeof newPat, "\t%s='%s'", "dbTableName", table);
	char *newerCtLine = replaceChars(result->newCtLine, oldPat, newPat);
	if (sameString(newerCtLine, result->newCtLine))
	    errAbort("Old name %s not found while swapping in new value for %s in new ct bed line", 
		origTableName, "dbTableName");
	result->newCtLine = newerCtLine;


	totalSize += uploadFileIfExists(dbDir, track, ".wib" , result, backgroundProgress, dyProg);
	totalSize += uploadFileIfExists(dbDir, track, ".maf" , result, backgroundProgress, dyProg);
	// vcf patches table data trash pointer, depends on dbTableName being patched already above
	totalSize += uploadFileIfExists(dbDir, track, ".vcf" , result, backgroundProgress, dyProg); 

	// save new table size in result (after vcf which can make small changes)
	long tableSize = sqlTableDataSizeFromSchema(ctConn, CUSTOM_TRASH, table);
	totalSize += tableSize;
	hFreeConn(&ctConn);
	}
    totalSize += uploadFileIfExists(dbDir, track, ".html", result, backgroundProgress, dyProg);

    slAddHead(&results, result);
    }
slReverse(&results);

//  read the contents of the saved session.
//  fetch the value for the given db.
//  make a new ct.bed merging in the results.
//  create new output copying the new merged one it its place
//  update the saved session contents.

processCtsForUploadInternals(pContents, dbDir, db, results, newDb);

// use results to update ct
// loop over results results to patch latest ct.bed

// return the sums of new sizes.
return totalSize;
}

boolean testArchCtBed(char *dbDir)
/* Test if ct.bed exists. */
{
char ctPath[1024];
safef(ctPath, sizeof ctPath, "%s/ct.bed", dbDir);
return fileExists(ctPath);
}


void removeArchiveCtBed(char *dbDir)
/* this acts as a signal that there is nothing here,
 * and when we are making the cart, that we should delete
 * the corresponding CT_PREFIX cart var.*/
{
char ctPath[1024];
safef(ctPath, sizeof ctPath, "%s/ct.bed", dbDir);
if (fileExists(ctPath))
    unlink(ctPath);
}

long uploadArchive(char **pContents, char *archDir, char *backgroundProgress, struct dyString *dyProg)
/* Show archive dir. Return total size. */
{
long totalSize = 0;
struct fileInfo *fI = NULL, *fIList = listDirX(archDir, "*", FALSE);
for(fI=fIList; fI; fI=fI->next)
    {
    if (fI->isDir)
	{
	char *db = fI->name;
	char dbDir[1024];
	safef(dbDir, sizeof dbDir, "%s/%s", archDir, db);
	char *newDb = NULL;
	if (startsWith("hub_", db))
	    {
	    newDb = getNewHub(dbDir, db, FALSE);
	    if (!newDb)
		{
		removeArchiveCtBed(dbDir);  // will trigger skipping 
		}
	    }
	else
	    {
	    if (!isActiveDb(db))
		{
		removeArchiveCtBed(dbDir);  // will trigger skipping 
		}
	    newDb = cloneString(db);
	    }

	if (!testArchCtBed(dbDir))
	    {
	    // newDb can be NULL for broken hub
	    // cause removal from cart unused CT_PREFIX vars
	    processCtsForUploadInternals(pContents, dbDir, db, NULL, newDb); 
	    continue; // skip if hub not active or db not found.
	    }

	if (!newDb)
	    errAbort("unexpected error newDb is null in uploadArchive.");

    	printf("<h3>%s</h3>\n", db);
	dyStringPrintf(dyProg, "<h3>%s</h3>\n", db);
	updateProgessFile(backgroundProgress, dyProg);
	lazarusLives(20 * 60);

	totalSize += uploadDb(pContents, dbDir, db, newDb, backgroundProgress, dyProg);

	}
    
    }
return totalSize;
}


void doUploadSessionCtData(char *param1, char *backgroundProgress)
/* Extract uploaded archive to restore cts. */
{

// Initialize .progress channel file
struct dyString *dyProg = newDyString(256);
dyStringPrintf(dyProg, "please wait, ...<br>\n");
updateProgessFile(backgroundProgress, dyProg);
lazarusLives(20 * 60);

htmlOpen("Restoring Settings and Custom Tracks from Archive");

printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=POST "
       "ENCTYPE=\"multipart/form-data\">\n",
       hgSessionName());
cartSaveSession(cart);

char *encRandPath = cloneString(param1 + strlen(hgsDoUploadPrefix));
char *randPath = cgiDecodeClone(encRandPath);

// setup temp output dir:
// ../trash/
//  ctRestore/
//   random@#$%/  # random value so it is unique

/* make a directory, including parent directories */

char tempOut[1024];
safef(tempOut, sizeof tempOut, "../trash/ctRestore");
if (!fileExists(tempOut))
    {
    makeDir(tempOut);
    }

// use randomDir 
char tempOutRand[1024];
safef(tempOutRand, sizeof tempOutRand, "%s/%s", tempOut, randPath);


// read cart file
char cartPath[1024];
safef(cartPath, sizeof cartPath, "%s/cart", tempOutRand); 
char *contents = NULL;

readInGulp(cartPath, &contents, NULL);

// Read original list of hub id and url for each in trackHubs var.
// This is needed when restoring the cart onto a foreign system such as a different mirror server.
char hubListFile[1024];
safef(hubListFile, sizeof hubListFile, "%s/hubList", tempOutRand);
struct lineFile *lf = lineFileOpen(hubListFile, TRUE);
// read each id with its URL to hubList file in archive root.
struct dyString *dy=dyStringNew(256);
char *line = NULL;
while (lineFileNext(lf, &line, NULL))
    {
    //split the line.
    char *space = strchr(line, ' ');
    if (!space)
	errAbort("unexected error reading hubList file: no space found in line.");
    *space = 0;
    //unsigned hubId = sqlUnsigned(line); // not needed now, but may be useful in the future.
    char *hubUrl = space+1;
    char *errMsg = NULL;
    unsigned newHubId = hubFindOrAddUrlInStatusTable(NULL, NULL, hubUrl, &errMsg);
    /* find this url in the status table, and return its id and errorMessage (if an errorMessage exists) */
    if ((!errMsg) && (newHubId != 0))
	{
	if (dy->stringSize > 0)
	    dyStringAppendC(dy, ' ');
	dyStringPrintf(dy, "%d", newHubId);
	}
    }
lineFileClose(&lf);
// update trackHubs variable in contents.
processContentsForTrackHubs(&contents, dy->string);

long totalSize = uploadArchive(&contents, tempOutRand, backgroundProgress, dyProg);

// save cart to db
saveContentsToTable(contents, cartSessionRawId(cart), "sessionDb");
saveContentsToTable(contents, cartUserRawId(cart), "userDb");
    
printf("<br>\n");


char greek[32];
sprintWithGreekByte(greek, sizeof(greek), totalSize);
printf("Total Size restored %s ", greek);

// Add a button that just returns to the saved sessions list.
// because after pressing upload,
// the browser remains sitting on this page and needs a way to return
// to the saved sessions list.
printf(" &nbsp; ");
    printf("<input type='submit' value='return'>");

printf("<br>\n");
printf("<br>\n");

printf("</FORM>\n");

htmlClose();
fflush(stdout);

}

