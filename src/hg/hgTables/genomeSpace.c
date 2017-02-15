/* genomeSpace - stuff related to GenomeSpace. */

#include "common.h"
#include "hgTables.h"
#include "cart.h"
#include "net.h"
#include "textOut.h"
#include "base64.h"
#include "md5.h"
#include "obscure.h"
#include "net.h"
#include "hgConfig.h"

#include <sys/wait.h>

// Declare external global variables that must be reset when
// before outputting a new page.  Used for outputting multiple pages.
extern boolean webHeadAlreadyOutputed;
extern boolean webInTextMode;
extern struct hash *includedResourceFiles;
extern boolean htmlWarnBoxSetUpAlready;
// note there is also an inWeb boolean in cart.c 
//  that would have needed resetting, but I added a line
//  in webEnd() to reset it.

boolean doGenomeSpace()
/* has the send to GenomeSpace checkbox been selected? */
{
return cartUsualBoolean(cart, "sendToGenomeSpace", FALSE);
}

static void showMissingOutputFileForm()
/* User needs to specify the output file */
{
htmlOpen("GenomeSpace");
printf("Please specify the output file field for GenomeSpace Data Manager.");
printf("<br>");
printf("<br>");
// TODO handle filename with a path.
// ACTUALLY, this probably just works.
printf("Your output file name may contain a path.");
printf("<br>");
printf("<br>");
printf("<FORM ACTION=\"/cgi-bin/hgTables\" METHOD=GET>"
   "<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"Back\" ></FORM>", hgtaDoMainPage);
htmlClose();
}


static void showGsLoginForm()
/* User needs to login to GS */
{
// TODO should this be a redirect? 
// TODO should it require https? - note our apache virtual hosts are not set up to work with it yet?
// GS Login Page
htmlOpen("GenomeSpace");
printf("Please login to GenomeSpace.");
printf("<br>");
printf("<br>");
printf("<FORM ACTION=\"/cgi-bin/hgTables\" METHOD=POST>");
printf("<table>");
printf("<tr><td align=right><B>User:</B></td><td><INPUT TYPE=TEXT NAME=\"%s\" SIZE=20 VALUE=\"\"></td></tr>", hgtaGsUser);
printf("<tr><td><B>Password:</B></td><td><INPUT TYPE=PASSWORD NAME=\"%s\" SIZE=20 VALUE=\"\"></td></tr>", hgtaGsPassword);
printf("<tr><td>&nbsp;</td><td><INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"Login to GenomeSpace\"></td></tr>", hgtaDoGsLogin);
printf("</form>");
printf("<tr><td>&nbsp;</td><td><FORM ACTION=\"/cgi-bin/hgTables\" METHOD=GET>"
	"<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"Cancel\" ></FORM></td></tr>", hgtaDoMainPage);
printf("</table>");
htmlClose();
}

static char *parseResponse(int sd, char **pResponseCode)
/* parse the http response */
{
struct dyString *dy = netSlurpFile(sd);
close(sd);

char *protocol = "HTTP/1.1 ";
if (!startsWith(protocol, dy->string))
    errAbort("GenomeSpace: Expected response to start with [%s], got [%s]", protocol, dy->string);

if (pResponseCode)
    {
    char *rc = dy->string + strlen(protocol);
    char *rcEndString =  "\r\n";
    char *rcEnd = strstr(dy->string, rcEndString);
    *pResponseCode = cloneStringZ(rc, rcEnd - rc);
    }

char *headerEndString =  "\r\n\r\n";
char *headerEnd = strstr(dy->string, headerEndString);
if (!headerEnd)
    errAbort("header end not found in response");
char *gsResponse = cloneString(headerEnd+strlen(headerEndString));

dyStringFree(&dy);

return gsResponse;

}

static char *getGenomeSpaceConfig(char *variable)
/* Read genomeSpace config setting or abort if not found */
{
char *value = cfgOption2("genomeSpace", variable);
return value;
}

boolean isGenomeSpaceEnabled()
/* genomeSpace is enabled by the presence of GS config settings. */
{
char *iSU = getGenomeSpaceConfig("identityServerUrl");
char *dmSvr = getGenomeSpaceConfig("dmServer");
if (isNotEmpty(iSU) && isNotEmpty(dmSvr))
    return TRUE;
return FALSE;
}

char *insertUserPasswordIntoUrl(char *url, char *user, char *password)
/* Insert cgi-encoded user and password into url after protocol. Free returned string when done. */
{
char resultUrl[1024];
char *encUser = cgiEncode(user);
char *encPassword = cgiEncode(password);
char *rest = stringIn("://", url);
if (!rest)
    errAbort("expected url [%s] to have ://", url);
char *protocol = cloneStringZ(url, rest - url);
rest += strlen("://");
safef(resultUrl, sizeof resultUrl, "%s://%s:%s@%s", protocol, encUser, encPassword, rest);

freeMem(protocol);
freeMem(encUser);
freeMem(encPassword);

return cloneString(resultUrl);
}

static char *getAuthorizationToken(char *user, char *password)
/* Authenticate against GenomeSpace 
 * Returns a token like [IGYpFc1CNO7acOJicopKHBTCS6JwDgoy]*/
{

//old url: safef(authUrl, sizeof authUrl, "https://%s:%s@identity.genomespace.org/identityServer/basic", encUser, encPassword);
//old2: safef(authUrl, sizeof authUrl, "https://%s:%s@identitytest.genomespace.org:8443/identityServer/basic", encUser, encPassword);
//old3: safef(authUrl, sizeof authUrl, "https://%s:%s@identity.genomespace.org/identityServer/basic", encUser, encPassword);

char *iSU = getGenomeSpaceConfig("identityServerUrl");
char *authUrl = insertUserPasswordIntoUrl(iSU, user, password);

int sd = netUrlOpen(authUrl);
if (sd < 0)
    errAbort("failed to open socket for [%s]", authUrl);
char *responseCode = NULL;
char *authToken = parseResponse(sd, &responseCode);
if (startsWith("401 ", responseCode))
    return NULL;
if (!sameString(responseCode, "200 OK"))
    errAbort("GenomeSpace getAuthorizationToken: %s", responseCode);

freeMem(authUrl);

return authToken;
}

static char *getGsPersonalDirectory(char *gsToken)
/* Get User's default directory from GenomeSpace DM 
 * Returns a url like [https://identity.genomespace.org/datamanager/files/users/<user>]    
 */
{
// DEFAULT DIRECTORY

// old1 char *defaultDirectoryUrl = "https://identity.genomespace.org/datamanager/defaultdirectory";
// old2 char *defaultDirectoryUrl = "https://dmtest.genomespace.org:8444/datamanager/defaultdirectory";
// old3 char *defaultDirectoryUrl = "https://dm.genomespace.org/datamanager/v1.0/defaultdirectory";
// NOTE the defaultdirectory method got renamed to personaldirectory
// old4 char *personalDirectoryUrl = "https://dm.genomespace.org/datamanager/v1.0/personaldirectory";

char *dmSvr = getGenomeSpaceConfig("dmServer");
char personalDirectoryUrl[1024];
safef(personalDirectoryUrl, sizeof personalDirectoryUrl, "%s/v1.0/personaldirectory", dmSvr);
    
struct dyString *reqExtra = newDyString(256);
dyStringPrintf(reqExtra, "Cookie: gs-token=%s\r\n", gsToken);
    
int sd = netOpenHttpExt(personalDirectoryUrl, "GET", reqExtra->string);
if (sd < 0)
    errAbort("failed to open socket for [%s]", personalDirectoryUrl);

struct dyString *dy = netSlurpFile(sd);
close(sd);

char *personalDirectory = NULL;

if (strstr(dy->string, "HTTP/1.1 303 See Other"))
    {
    char *valStart = strstr(dy->string, "Location: ");
    if (valStart)
	{
	valStart += strlen("Location: ");
	char *valEnd = strstr(valStart, "\r\n");
	if (!valEnd)
	    errAbort("location not found in response headers");
	personalDirectory = cloneStringZ(valStart, valEnd - valStart);
	}
    }    
dyStringFree(&dy);
dyStringFree(&reqExtra);

return personalDirectory;

}


boolean checkGsReady()
/* check that GS requirements are met */
{
// check that the output file has been specified
char *fileName = cartUsualString(cart, hgtaOutFileName, "");
if (sameString(fileName,""))
    {
    cartRemove(cart, hgtaDoTopSubmit);
    showMissingOutputFileForm();
    return FALSE;
    }
// check login
// is the GS login token in the cart?
char *gsToken = cartUsualString(cart, "gsToken", NULL);
if (!gsToken)
    { 
    cartRemove(cart, hgtaDoTopSubmit);
    showGsLoginForm();
    return FALSE;
    }
else
    {   
    // check if the token still valid
    char *temp = getGsPersonalDirectory(gsToken);
    if (!temp)
	{
	cartRemove(cart, hgtaDoTopSubmit);
    	showGsLoginForm();
	return FALSE;
	}
    freeMem(temp);
    }
return TRUE; 
}


void doGsLogin(struct sqlConnection *conn)
/* Process user password post.
 * Log into GS 
 * if successful save gsToken 
 * else return to login page or to mainpage */
{
char *user = cloneString(cartUsualString(cart, hgtaGsUser, NULL));
char *password = cloneString(cartUsualString(cart, hgtaGsPassword, NULL));
// do not leave them in the cart
cartRemove(cart, hgtaGsUser);
cartRemove(cart, hgtaGsPassword);

if (!(user && password))
    errAbort("expecting GenomeSpace user and password");

char *gsToken = getAuthorizationToken(user, password);

if (gsToken)
    {
    cartSetString(cart, "gsToken", gsToken);
    }
else
    {
    cartRemove(cart, "gsToken");
    }

cartSetString(cart, hgtaDoTopSubmit, "get output");

}

char *gsUploadUrl(char *gsToken, char *user, char *uploadFileName, off_t contentLength, char *base64Md5, char *contentType)
/* call uploadurl */
{
// UPLOADURLS 

// TODO deal with creating parent dirs if uploadFileName contains a path? maybe not.

// old:  "https://identity.genomespace.org/datamanager/uploadurls/users/"
// old     "https://dm.genomespace.org/datamanager/v1.0/uploadurl/users/"  // if this works, use default dir fetched earlier instead

char *dmSvr = getGenomeSpaceConfig("dmServer");
char uploadUrl[1024];
safef(uploadUrl, sizeof(uploadUrl),
    "%s/v1.0/uploadurl/users/"
    "%s/"
    "%s"
    "?Content-Length=%lld"
    "&Content-MD5=%s"
    "&Content-Type=%s"
    , dmSvr
    , user
    , uploadFileName
    , (long long) contentLength
    , cgiEncode(base64Md5)
    , contentType
    );


struct dyString *reqExtra = newDyString(256);
dyStringPrintf(reqExtra, "Cookie: gs-token=%s\r\n", gsToken);

int sd = netOpenHttpExt(uploadUrl, "GET", reqExtra->string);
if (sd < 0)
    errAbort("failed to open socket for [%s]", uploadUrl);

char *responseCode = NULL;
char *s3UploadUrl = parseResponse(sd, &responseCode);
if (sameString(responseCode, "404 Not Found"))
    errAbort("GenomeSpace: %s, if a path was used in the output name, it may indicate the path does not exist in GenomeSpace.", responseCode);
if (!sameString(responseCode, "200 OK"))
    errAbort("GenomeSpace: %s", responseCode);

dyStringFree(&reqExtra);

return s3UploadUrl;

}



#define S3UPBUFSIZE 65536
char *gsS3Upload(char *s3UploadUrl, char *inputFileName, off_t contentLength, char *base64Md5, char *hexMd5, char *contentType, boolean progress, char *fileName)
/* call s3 upload */
{
// S3 UPLOAD  to Amazon Storage

struct dyString *reqExtra = newDyString(256);
dyStringPrintf(reqExtra, "Content-Length: %lld\r\n", (long long)contentLength);
dyStringPrintf(reqExtra, "Content-MD5: %s\r\n", base64Md5);
dyStringPrintf(reqExtra, "Content-Type: %s\r\n", contentType);

int sd = netOpenHttpExt(s3UploadUrl, "PUT", reqExtra->string);
if (sd < 0)
    errAbort("failed to open socket for [%s]", s3UploadUrl);


unsigned char buffer[S3UPBUFSIZE];
int bufRead = 0;
FILE *f = mustOpen(inputFileName,"rb");
off_t totalUploaded = 0;
int lastPctUploaded = -1;
// upload the file contents
while ((bufRead = fread(&buffer, 1, S3UPBUFSIZE, f)) > 0) 
    {
    int bufWrite = 0;
    while (bufWrite < bufRead)
	{
	int socketWrite = write(sd, buffer + bufWrite, bufRead - bufWrite);
	if (socketWrite == -1)
	    {
	    if (errno == 32) // broken pipe often happens when the ssh connection shuts down or has errors.
		{
		warn("broken pipe, S3 server closed the ssh connection.");
		break;
		}
	    errnoAbort("error writing to socket for GenomeSpace upload");
	    }
	bufWrite += socketWrite;
	}
    if (errno == 32)
	break;
    totalUploaded += bufRead;
    int pctUploaded = 100.0*totalUploaded/contentLength;
    if (progress && (pctUploaded != lastPctUploaded))
	{

	char nicenumber[1024]="";
	sprintWithGreekByte(nicenumber, sizeof(nicenumber), contentLength);

	// Various global flags must be reset to draw a fresh html output page.
	webHeadAlreadyOutputed = FALSE;
	webInTextMode = FALSE;
	includedResourceFiles = NULL;
	htmlWarnBoxSetUpAlready=FALSE;
	jsInlineReset();

	htmlOpen("Uploading Output to GenomeSpace");

	printf("Name: %s<br>\n", fileName);
	printf("Size: %s<br>\n", nicenumber);
	printf("Progress: %0d%%<br>\n", pctUploaded);
	printf("<br>\n");

	printf("<FORM ACTION=\"/cgi-bin/hgTables\" METHOD=GET>\n"
	    "<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"Back\" >"
	    "<INPUT TYPE=SUBMIT NAME=\"Refresh\" id='Refresh' VALUE=\"Refresh\">"
	    "</FORM>\n"
	    , hgtaDoMainPage);
	jsOnEventById("click", "Refresh", "window.location=window.location;return false;");
	jsInline("setTimeout(function(){location = location;},5000);\n");

	htmlClose();
	fflush(stdout);

	lastPctUploaded = pctUploaded;

	}
    }

carefulClose(&f);

char *responseCode = NULL;
char *s3UploadResponse = parseResponse(sd, &responseCode);
if (!sameString(responseCode, "200 OK"))
    errAbort("Amazon S3 Response: %s", responseCode);

dyStringFree(&reqExtra);

return s3UploadResponse;

}


void getBackgroundStatus(char *url)
/* fetch status as the latest complete html block available */
{
char *html = NULL;
if (fileSize(url)==0)
    {
    htmlOpen("Background Status");
    errAbort("No output found. Expecting output in [%s].", url);
    htmlClose();
    return;
    }

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
    printf("%s", textErr);
    }
}

#include "trashDir.h"
// TODO move this to a generic re-usable location
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

	// execute so that we will be able to use database and other operations normally.
	char execPath[4096];
	safef(execPath, sizeof execPath, "%s hgsid=%s", exec, hgsid);
	char *args[10];
	int numArgs = chopString(execPath, " ", args, 10);
	args[numArgs] = NULL;
	// by creating a minimal environment and not inheriting from the parent,
	// it cause cgiSpoof to run,  picking up command-line params as cgi vars.
	char *newenviron[] = { "HGDB_CONF=hg.conf", NULL };
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


void gsSendToDM()
/* upload the generated file to DM */
{
// This is now run via fork/exec as a separate background process.

char *trashFileName = cartUsualString(cart, "gsTemp", "");
char *fileName = cartUsualString(cart, hgtaOutFileName, "");

// adjust upload name based on compression and existing extension
char *compressType = cartUsualString(cart, hgtaCompressType, textOutCompressNone);

if (!(isEmpty(compressType) || sameWord(compressType, textOutCompressNone)))
    {
    char *suffix = getCompressSuffix(compressType);
    if (!endsWith(fileName, suffix))
	fileName = addSuffix(fileName, suffix);
    }


off_t fSize = fileSize(trashFileName);


char *gsToken = cartUsualString(cart, "gsToken", NULL);

char *contentType = "text/plain";  // some examples show applicaton/octet-stream

char *persDir = getGsPersonalDirectory(gsToken);
char *user = strrchr(persDir,'/');
++user;

char nicenumber[1024]="";
sprintWithGreekByte(nicenumber, sizeof(nicenumber), fSize);

htmlOpen("Uploading Output to GenomeSpace");

printf("Name: %s<br>\n", fileName);
printf("Size: %s<br>\n", nicenumber);
printf("Progress: 0%%<br>\n");
printf("You can remain on this page and monitor upload progress.<br>\n");
printf("Otherwise, feel free to continue working, and your output will appear in GenomeSpace when the upload is complete.<br>\n");
printf("<br>\n");
printf("<FORM ACTION=\"/cgi-bin/hgTables\" METHOD=GET>\n"
        "<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"Back\" >\n"
	"<INPUT TYPE=SUBMIT NAME=\"Refresh\" id='Refresh' VALUE=\"Refresh\">"
	"</FORM>\n"
	, hgtaDoMainPage);
jsOnEventById("click", "Refresh", "window.location=window.location;return false;");
jsInline("setTimeout(function(){location = location;},5000);\n");

htmlClose();
fflush(stdout);

// MD5 COMPUTE
unsigned char md5[16];       /* Keep the md5 checksum here. */
md5ForFile(trashFileName,md5);
char *hexMd5 = md5ToHex(md5);
char *base64Md5 = base64Encode((char*)md5, 16);


char *s3UploadUrl = gsUploadUrl(gsToken, user, fileName, fSize, base64Md5, contentType);

char *s3Response = gsS3Upload(s3UploadUrl, trashFileName, fSize, base64Md5, hexMd5, contentType, TRUE, fileName);
    
if (sameString(s3Response,""))
    {
    // Reset global flags before drawing brand new page
    webHeadAlreadyOutputed = FALSE;
    webInTextMode = FALSE;
    includedResourceFiles = NULL;
    htmlWarnBoxSetUpAlready=FALSE;
    jsInlineReset();

    htmlOpen("Uploaded Output to GenomeSpace");

    printf("Name: %s<br>\n", fileName);
    printf("Size: %s<br>\n", nicenumber);
    printf("Output has been successfully uploaded.<br>\n");
    printf("<br>");
    printf("<FORM ACTION=\"/cgi-bin/hgTables\" METHOD=GET>\n"
        "<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"Back\" ></FORM>\n"
	, hgtaDoMainPage);
    htmlClose();
    fflush(stdout);
    }

//printf("s3UploadUrl [%s]", s3UploadUrl);
//printf("<br>");
//printf("s3Response [%s]", s3Response);
//printf("<br>");

exit(0);  // CANNOT RETURN

}
