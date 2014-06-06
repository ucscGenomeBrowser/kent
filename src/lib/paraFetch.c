/* paraFetch - fetch things from remote URLs in parallel. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include <utime.h>
#include "common.h"
#include "internet.h"
#include "errAbort.h"
#include "hash.h"
#include "linefile.h"
#include "net.h"
#include "https.h"
#include "sqlNum.h"
#include "obscure.h"
#include "portable.h"
#include "paraFetch.h"


static void paraFetchWriteStatus(char *origPath, struct parallelConn *pcList, 
    char *url, off_t fileSize, char *dateString, boolean isFinal)
/* Write a status file.
 * This has two purposes.
 * First, we can use it to resume a failed transfer.
 * Second, we can use it to follow progress */
{
char outTempX[1024];
char outTemp[1024];
safef(outTempX, sizeof(outTempX), "%s.paraFetchStatusX", origPath);
safef(outTemp, sizeof(outTemp), "%s.paraFetchStatus", origPath);
struct parallelConn *pc = NULL;

FILE *f = mustOpen(outTempX, "w");
int part = 0;
fprintf(f, "%s\n", url);
fprintf(f, "%lld\n", (long long)fileSize);
fprintf(f, "%s\n", dateString);
for(pc = pcList; pc; pc = pc->next)
    {
    fprintf(f, "part%d %lld %lld %lld\n", part
	, (long long)pc->rangeStart
	, (long long)pc->partSize
	, (long long)pc->received);
    ++part;
    }

carefulClose(&f);

/* rename the successful status to the original name */
rename(outTempX, outTemp);

if (isFinal)  /* We are done and just looking to get rid of the file. */
    unlink(outTemp);
}


time_t paraFetchTempUpdateTime(char *origPath)
/* Return last mod date of temp file - which is useful to see if process has stalled. */
{
char outTemp[1024];
safef(outTemp, sizeof(outTemp), "%s.paraFetch", origPath);
if (fileExists(outTemp))
    return fileModTime(outTemp);
else if (fileExists(origPath))
    return fileModTime(origPath);
else
    {
    errAbort("%s doesn't exist", origPath);
    return 0;
    }
}

void parallelFetchRemovePartial(char *destName)
/* Remove any files associated with partial downloads of file of given name. */
{
char paraTmpFile[PATH_LEN];
safef(paraTmpFile, PATH_LEN, "%s.paraFetch", destName);
remove(paraTmpFile);
safef(paraTmpFile, PATH_LEN, "%s.paraFetchStatus", destName);
remove(paraTmpFile);
}

boolean paraFetchReadStatus(char *origPath, 
    struct parallelConn **pPcList, char **pUrl, off_t *pFileSize, 
    char **pDateString, off_t *pTotalDownloaded)
/* Read a status file - which is just origPath plus .paraFetchStatus.  This is updated during 
 * transit by parallelFetch. Returns FALSE if status file not there - possibly because
 * transfer is finished.  Any of the return parameters (pThis and pThat) may be NULL */
{
char outTemp[1024];
char outStat[1024];
safef(outStat, sizeof(outStat), "%s.paraFetchStatus", origPath);
safef(outTemp, sizeof(outTemp), "%s.paraFetch", origPath);
struct parallelConn *pcList = NULL, *pc = NULL;
off_t totalDownloaded = 0;

if (!fileExists(outStat))
    {
    unlink(outTemp);
    return FALSE;
    }

if (!fileExists(outTemp))
    {
    unlink(outStat);
    return FALSE;
    }

char *line, *word;
struct lineFile *lf = lineFileOpen(outStat, TRUE);
if (!lineFileNext(lf, &line, NULL))
    {
    unlink(outTemp);
    unlink(outStat);
    return FALSE;
    }
char *url = cloneString(line);
if (!lineFileNext(lf, &line, NULL))
    {
    unlink(outTemp);
    unlink(outStat);
    return FALSE;
    }
off_t fileSize = sqlLongLong(line);
if (!lineFileNext(lf, &line, NULL))
    {
    unlink(outTemp);
    unlink(outStat);
    return FALSE;
    }
char *dateString = cloneString(line);
while (lineFileNext(lf, &line, NULL))
    {
    word = nextWord(&line);
    AllocVar(pc);
    pc->next = NULL;
    pc->sd = -4;  /* no connection tried yet */
    word = nextWord(&line);
    pc->rangeStart = sqlLongLong(word);
    word = nextWord(&line);
    pc->partSize = sqlLongLong(word);
    word = nextWord(&line);
    pc->received = sqlLongLong(word);
    if (pc->received == pc->partSize)
	pc->sd = -1;  /* part all done already */
    totalDownloaded += pc->received;
    slAddHead(&pcList, pc);
    }
slReverse(&pcList);

lineFileClose(&lf);

if (slCount(pcList) < 1)
    {
    unlink(outTemp);
    unlink(outStat);
    return FALSE;
    }

if (pPcList != NULL)
    *pPcList = pcList;
if (pUrl != NULL)
    *pUrl = url;
if (pFileSize != NULL)
    *pFileSize = fileSize;
if (pDateString != NULL)
    *pDateString = dateString;
if (pTotalDownloaded != NULL)
    *pTotalDownloaded = totalDownloaded;

return TRUE;
}

boolean parallelFetchInterruptable(char *url, char *outPath, int numConnections, int numRetries, 
    boolean newer, boolean progress,
    boolean (*interrupt)(void *context),  void *context)
/* Open multiple parallel connections to URL to speed downloading.  If interrupt function 
 * is non-NULL,  then it gets called passing the context parameter,  and if it returns
 * TRUE the fetch is interrupted.   Overall the parallelFetchInterruptable returns TRUE
 * when the function succeeds without interrupt, FALSE otherwise. */
{
char *origPath = outPath;
char outTemp[1024];
safef(outTemp, sizeof(outTemp), "%s.paraFetch", outPath);
outPath = outTemp;
/* get the size of the file to be downloaded */
off_t fileSize = 0;
off_t totalDownloaded = 0;
ssize_t sinceLastStatus = 0;
char *dateString = "";
int star = 1;  
int starMax = 20;  
off_t starStep = 1;
// TODO handle case-sensitivity of protocols input
if (startsWith("http://",url) || startsWith("https://",url))
    {
    struct hash *hash = newHash(0);
    int status = netUrlHead(url, hash);
    if (status != 200) // && status != 302 && status != 301)
	{
	warn("Error code: %d, expected 200 for %s, can't proceed, sorry", status, url);
	return FALSE;
	}
    char *sizeString = hashFindValUpperCase(hash, "Content-Length:");
    if (sizeString)
	{
	fileSize = atoll(sizeString);
	}
    else
	{
	warn("No Content-Length: returned in header for %s, must limit to a single connection, will not know if data is complete", url);
	numConnections = 1;
	fileSize = -1;
	}
    char *ds = hashFindValUpperCase(hash, "Last-Modified:");
    if (ds)
	dateString = cloneString(ds);
    hashFree(&hash);
    }
else if (startsWith("ftp://",url))
    {
    long long size = 0;
    time_t t;
    boolean ok = netGetFtpInfo(url, &size, &t);
    if (!ok)
	{
	warn("Unable to get size info from FTP for %s, can't proceed, sorry", url);
	return FALSE;
	}
    fileSize = size;

    struct tm  *ts;
    char ftpTime[80];
 
    /* Format the time "Tue, 15 Jun 2010 06:45:08 GMT" */
    ts = localtime(&t);
    strftime(ftpTime, sizeof(ftpTime), "%a, %d %b %Y %H:%M:%S %Z", ts);
    dateString = cloneString(ftpTime);

    }
else
    {
    warn("unrecognized protocol: %s", url);
    return FALSE;
    }


verbose(2,"fileSize=%lld\n", (long long) fileSize);

if (fileSize < 65536)    /* special case small file */
    numConnections = 1;

if (numConnections > 50)    /* ignore high values for numConnections */
    {
    warn("Currently maximum number of connections is 50. You requested %d. Will proceed with 50 on %s", numConnections, url);
    numConnections = 50;
    }

verbose(2,"numConnections=%d\n", numConnections); //debug

if (numConnections < 1)
    {
    warn("number of connections must be greater than 0 for %s, can't proceed, sorry", url);
    return FALSE;
    }

if (numRetries < 0)
    numRetries = 0;

/* what is the size of each part */
off_t partSize = (fileSize + numConnections -1) / numConnections;
if (fileSize == -1) 
    partSize = -1;
off_t base = 0;
int c;

verbose(2,"partSize=%lld\n", (long long) partSize); //debug


/* n is the highest-numbered descriptor */
int n = 0;
int connOpen = 0;
int reOpen = 0;


struct parallelConn *restartPcList = NULL;
char *restartUrl = NULL;
off_t restartFileSize = 0;
char *restartDateString = "";
off_t restartTotalDownloaded = 0;
boolean restartable = paraFetchReadStatus(origPath, &restartPcList, &restartUrl, &restartFileSize, &restartDateString, &restartTotalDownloaded);
if (fileSize == -1)
    restartable = FALSE;

struct parallelConn *pcList = NULL, *pc;

if (restartable 
 && sameString(url, restartUrl)
 && fileSize == restartFileSize
 && sameString(dateString, restartDateString))
    {
    pcList = restartPcList;
    totalDownloaded = restartTotalDownloaded;
    }
else
    {

    if (newer) // only download it if it is newer than what we already have
	{
	/* datestamp mtime from last-modified header */
	struct tm tm;
	// Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT
	// These strings are always GMT
	if (strptime(dateString, "%a, %d %b %Y %H:%M:%S %Z", &tm) == NULL)
	    {
	    warn("unable to parse last-modified string [%s]", dateString);
	    }
	else
	    {
	    time_t t;
	    // convert to UTC (GMT) time
	    t = mktimeFromUtc(&tm);
	    if (t == -1)
		{
		warn("mktimeFromUtc failed while converting last-modified string to UTC [%s]", dateString);
		}
	    else
		{
		// get the file mtime
		struct stat mystat;
		ZeroVar(&mystat);
		if (stat(origPath,&mystat)==0)
		    {
		    if (t <= mystat.st_mtime)
			{
			verbose(2,"Since nothing newer was found, skipping %s\n", origPath);
			verbose(3,"t from last-modified = %ld; st_mtime = %ld\n", (long) t, (long)mystat.st_mtime);
			return TRUE;
			}
		    }
		}
	    }
	}

    /* make a list of connections */
    for (c = 0; c < numConnections; ++c)
	{
	AllocVar(pc);
	pc->next = NULL;
	pc->rangeStart = base;
	base += partSize;
	pc->partSize = partSize;
	if (fileSize != -1 && pc->rangeStart+pc->partSize >= fileSize)
	    pc->partSize = fileSize - pc->rangeStart;
	pc->received = 0;
	pc->sd = -4;  /* no connection tried yet */
	slAddHead(&pcList, pc);
	}
    slReverse(&pcList);
    }

if (progress)
    {
    char nicenumber[1024]="";
    sprintWithGreekByte(nicenumber, sizeof(nicenumber), fileSize);
    printf("downloading %s ", nicenumber); fflush(stdout);
    starStep = fileSize/starMax;
    if (starStep < 1)
	starStep = 1;
    }

int out = open(outPath, O_CREAT|O_WRONLY, 0664);
if (out < 0)
    {
    warn("Unable to open %s for write while downloading %s, can't proceed, sorry", outPath, url);
    return FALSE;
    }


/* descriptors for select() */
fd_set rfds;
struct timeval tv;
int retval;

ssize_t readCount = 0;
#define BUFSIZE 65536 * 4
char buf[BUFSIZE];

/* create paraFetchStatus right away for monitoring programs */
paraFetchWriteStatus(origPath, pcList, url, fileSize, dateString, FALSE);
sinceLastStatus = 0;

int retryCount = 0;

time_t startTime = time(NULL);

#define SELTIMEOUT 10
#define RETRYSLEEPTIME 30
int scaledRetries = numRetries;
if (fileSize != -1)
    scaledRetries = round(numRetries * (1+fileSize/1e10) );
verbose(2,"scaledRetries=%d\n", scaledRetries);
while (TRUE)
    {
    verbose(2,"Top of big loop\n");
    if (interrupt != NULL && (*interrupt)(context))
	{
	verbose(1, "Interrupted paraFetch.\n");
	return FALSE;
	}

    if (progress)
	{
	while (totalDownloaded >= star * starStep)
	    {
	    printf("*");fflush(stdout);
	    ++star;
	    }
	}

    /* are we done? */
    if (connOpen == 0)
	{
	boolean done = TRUE;
	for(pc = pcList; pc; pc = pc->next)
	    if (pc->sd != -1)
		done = FALSE;
	if (done) break;
	}

    /* See if we need to open any connections, either new or retries */
    for(pc = pcList; pc; pc = pc->next)
	{
	if ((pc->sd == -4)  /* never even tried to open yet */
	 || ((reOpen>0)      /* some connections have been released */
	    && (pc->sd == -2  /* started but not finished */
	    ||  pc->sd == -3)))  /* not even started */
	    {
	    char urlExt[1024];
	    safef(urlExt, sizeof(urlExt), "%s;byterange=%llu-%llu"
	    , url
	    , (unsigned long long) (pc->rangeStart + pc->received)
	    , (unsigned long long) (pc->rangeStart + pc->partSize - 1) );


	    int oldSd = pc->sd;  /* in case we need to remember where we were */
	    if (oldSd != -4)      /* decrement whether we succeed or not */
		--reOpen;
	    if (oldSd == -4) 
		oldSd = -3;       /* ok this one just changes */
	    if (fileSize == -1)
		{
		verbose(2,"opening url %s\n", url);
		pc->sd = netUrlOpen(url);
		}
	    else
		{
		verbose(2,"opening url %s\n", urlExt);
		pc->sd = netUrlOpen(urlExt);
		}
	    if (pc->sd < 0)
		{
		pc->sd = oldSd;  /* failed to open, can retry later */
		}
	    else
		{
		char *newUrl = NULL;
		int newSd = 0;
		if (startsWith("http://",url) || startsWith("https://",url))
		    {
		    if (!netSkipHttpHeaderLinesHandlingRedirect(pc->sd, urlExt, &newSd, &newUrl))
			{
			warn("Error processing http response for %s", url);
			return FALSE;
			}
		    if (newUrl) 
			{
			/*  Update sd with newSd, replace it with newUrl, etc. */
			pc->sd = newSd;
			}
		    }
		++connOpen;
		}
	    }
	}


    if (connOpen == 0)
	{
	warn("Unable to open any connections to download %s, can't proceed, sorry", url);
	return FALSE;
	}


    FD_ZERO(&rfds);
    n = 0;
    for(pc = pcList; pc; pc = pc->next)
	{
	if (pc->sd >= 0)
	    {
	    FD_SET(pc->sd, &rfds);    /* reset descriptor in readfds for select() */
	    if (pc->sd > n)
		n = pc->sd;
	    }
	}


    /* Wait up to SELTIMEOUT seconds. */
    tv.tv_sec = SELTIMEOUT;
    tv.tv_usec = 0;
    retval = select(n+1, &rfds, NULL, NULL, &tv);
    /* Don't rely on the value of tv now! */

    if (retval < 0)
	{
	perror("select retval < 0");
	return FALSE;
	}
    else if (retval)
	{

	verbose(2,"returned from select, retval=%d\n", retval);

	for(pc = pcList; pc; pc = pc->next)
	    {
	    if ((pc->sd >= 0) && FD_ISSET(pc->sd, &rfds))
		{

		verbose(2,"found a descriptor with data: %d\n", pc->sd);

		readCount = read(pc->sd, buf, BUFSIZE);

		verbose(2,"readCount = %lld\n", (long long) readCount);

		if (readCount == 0)
		    {
		    close(pc->sd);

		    verbose(2,"closing descriptor: %d\n", pc->sd);
		    pc->sd = -1;

		    if (fileSize != -1 && pc->received != pc->partSize)	
			{
			pc->sd = -2;  /* conn was closed before all data was sent, can retry later */
			}
		    --connOpen;
		    ++reOpen;
		    paraFetchWriteStatus(origPath, pcList, url, fileSize, dateString, FALSE);
		    sinceLastStatus = 0;
		    continue; 
		    }
		if (readCount < 0)
		    {
		    warn("error reading from socket for url %s", url);
		    return FALSE;
		    }

		verbose(2,"rangeStart %llu  received %llu\n"
			, (unsigned long long) pc->rangeStart
			, (unsigned long long) pc->received );

		verbose(2,"seeking to %llu\n", (unsigned long long) (pc->rangeStart + pc->received));

		if (lseek(out, pc->rangeStart + pc->received, SEEK_SET) == -1)
		    {
		    perror("error seeking output file");
		    warn("error seeking output file %s: rangeStart %llu  received %llu for url %s"
			, outPath
			, (unsigned long long) pc->rangeStart
			, (unsigned long long) pc->received
			, url);
		    return FALSE;
		    }
		int writeCount = write(out, buf, readCount);
		if (writeCount < 0)
		    {
		    warn("error writing output file %s", outPath);
		    return FALSE;
		    }
		pc->received += readCount;
		totalDownloaded += readCount;
		sinceLastStatus += readCount;
		if (sinceLastStatus >= 10*1024*1024)
		    {
		    paraFetchWriteStatus(origPath, pcList, url, fileSize, dateString, FALSE);
		    sinceLastStatus = 0;
		    }
		}
	    }
	}
    else
	{
	warn("No data within %d seconds for %s", SELTIMEOUT, url);
	/* Retry ? */
	if (retryCount >= scaledRetries)
	    {
    	    return FALSE;
	    }
	else
	    {
	    ++retryCount;
	    /* close any open connections */
	    for(pc = pcList; pc; pc = pc->next)
		{
		if (pc->sd >= 0) 
		    {
		    close(pc->sd);
		    verbose(2,"closing descriptor: %d\n", pc->sd);
		    }
		if (pc->sd != -1) 
		    pc->sd = -4;
		}
	    connOpen = 0;
	    reOpen = 0;
	    /* sleep for a while, maybe the server will recover */
	    sleep(RETRYSLEEPTIME);
	    }
	}

    }

close(out);

/* delete the status file - by passing TRUE */
paraFetchWriteStatus(origPath, pcList, url, fileSize, dateString, TRUE); 

/* restore original file datestamp mtime from last-modified header */
struct tm tm;
// Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT
// These strings are always GMT
if (strptime(dateString, "%a, %d %b %Y %H:%M:%S %Z", &tm) == NULL)
    {
    warn("unable to parse last-modified string [%s]", dateString);
    }
else
    {
    time_t t;
    // convert to UTC (GMT) time
    t = mktimeFromUtc(&tm);
    if (t == -1)
	{
	warn("mktimeFromUtc failed while converting last-modified string to UTC [%s]", dateString);
	}
    else
	{
	// update the file mtime
	struct utimbuf ut;
	struct stat mystat;
	ZeroVar(&mystat);
	if (stat(outTemp,&mystat)==0)
	    {
	    ut.actime = mystat.st_atime;
	    ut.modtime = t;
	    if (utime(outTemp, &ut)==-1)
		{
		char errMsg[256];
                safef(errMsg, sizeof(errMsg), "paraFetch: error setting modification time of %s to %s\n", outTemp, dateString);
		perror(errMsg);
		}
	    }
	}
    }

/* rename the successful download to the original name */
rename(outTemp, origPath);



if (progress)
    {
    while (star <= starMax)
	{
	printf("*");fflush(stdout);
	++star;
	}
    long timeDiff = (long)(time(NULL) - startTime);
    if (timeDiff > 0)
	{
	printf(" %ld seconds", timeDiff);
	float mbpersec =  ((totalDownloaded - restartTotalDownloaded)/1000000) / timeDiff;
	printf(" %0.1f MB/sec", mbpersec);
	}
    printf("\n");fflush(stdout);
    }

if (fileSize != -1 && totalDownloaded != fileSize)
    {
    warn("Unexpected result: Total downloaded bytes %lld is not equal to fileSize %lld"
	, (long long) totalDownloaded
	, (long long) fileSize);
    return FALSE;
    }
return TRUE;
}

boolean parallelFetch(char *url, char *outPath, int numConnections, int numRetries, 
    boolean newer, boolean progress)
/* Open multiple parallel connections to URL to speed downloading */
{
return parallelFetchInterruptable(url, outPath, numConnections, numRetries, 
    newer, progress, NULL, NULL);
}

