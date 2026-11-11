/* botDelayTester - what hgBotDelayTimeFracWithStatus() does when the bottleneck server
 * answers, refuses the connection, or accepts it and never answers.  refs #37738
 *
 * Before #37738 an unreachable bottleneck server made every CGI abort with a 500.
 * bottleneck.serveOnFailure=on now serves the request with the bottleneck.failureDelay
 * fallback and writes a line to the error log, and bottleneck.timeoutSeconds bounds the
 * connect, send and read, capped at 10 seconds.
 *
 * Each run writes its own hg.conf, sets HGDB_CONF to it and only then reads a setting,
 * because the config is read once per process.  The conf includes nothing, so no gate is
 * inherited from the developer's conf.  The fake bottleneck server is a forked child on
 * 127.0.0.1;  no network and no database is used.
 *
 * usage: botDelayTester mode gate timeoutSeconds failureDelay outDir
 *    mode     answer   the server answers 42
 *             refused  nothing listens on the port
 *             silent   the server accepts and never answers
 *    gate     on or off, for bottleneck.serveOnFailure
 *
 * Each run prints one line of result, and for "silent" a line saying whether the call
 * returned inside the window that the timeout allows.  The windows are wide (the call
 * waits 1 or 10 seconds against a server that holds the connection for 30), so a busy
 * machine cannot turn this red.  The error log line is printed with the port masked. */

#include "common.h"
#include "errCatch.h"
#include "net.h"
#include "botDelay.h"
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>

static int listenOnFreePort(int *retPort)
/* Open a listening socket on 127.0.0.1 with a port the kernel picks. */
{
int sd = socket(AF_INET, SOCK_STREAM, 0);
if (sd < 0)
    errnoAbort("socket");
struct sockaddr_in addr;
ZeroVar(&addr);
addr.sin_family = AF_INET;
addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
addr.sin_port = 0;
if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    errnoAbort("bind");
if (listen(sd, 4) < 0)
    errnoAbort("listen");
socklen_t len = sizeof(addr);
if (getsockname(sd, (struct sockaddr *)&addr, &len) < 0)
    errnoAbort("getsockname");
*retPort = ntohs(addr.sin_port);
return sd;
}

static pid_t startServer(int listenSd, boolean answer)
/* Fork a bottleneck server that takes one request.  If answer, it reports the check
 * string it got and sends back 42;  otherwise it holds the connection for 30 seconds. */
{
fflush(stdout);
fflush(stderr);
pid_t pid = fork();
if (pid < 0)
    errnoAbort("fork");
if (pid > 0)
    return pid;
int sd = accept(listenSd, NULL, NULL);
if (sd < 0)
    _exit(1);
if (answer)
    {
    char buf[256];
    if (netGetString(sd, buf) != NULL)
        printf("server got: %s\n", buf);
    fflush(stdout);
    netSendString(sd, "42");
    }
else
    sleep(30);
close(sd);
_exit(0);
}

static void writeConf(char *fileName, int port, char *gate, char *timeout, char *failureDelay)
/* Write an hg.conf that sets every setting the bottleneck code reads, by name. */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "bottleneck.host=127.0.0.1\n");
fprintf(f, "bottleneck.port=%d\n", port);
fprintf(f, "bottleneck.serveOnFailure=%s\n", gate);
fprintf(f, "bottleneck.timeoutSeconds=%s\n", timeout);
fprintf(f, "bottleneck.failureDelay=%s\n", failureDelay);
fprintf(f, "newBotDelay=off\n");
carefulClose(&f);
}

static void printLog(char *errFile, int port)
/* Print what the bottleneck code wrote to stderr.  Its own fallback line is printed with
 * the port replaced by PORT.  The warnings net.c raises on the way are only counted:  they
 * belong in the error log and not on the page, but their wording is net.c's, not ours. */
{
char portString[16];
safef(portString, sizeof(portString), ":%d", port);
int netWarnings = 0;
struct lineFile *lf = lineFileOpen(errFile, TRUE);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    if (startsWith("botDelay: no answer", line))
        {
        char *masked = replaceChars(line, portString, ":PORT");
        printf("  log: %s\n", masked);
        freeMem(masked);
        }
    else if (startsWith("botDelay: ", line))
        netWarnings++;
    }
lineFileClose(&lf);
printf("  net.c warnings sent to the log: %d\n", netWarnings);
}

int main(int argc, char *argv[])
{
if (argc != 6)
    errAbort("usage: botDelayTester answer|refused|silent on|off timeoutSeconds failureDelay outDir");
char *mode = argv[1], *gate = argv[2], *timeout = argv[3], *failureDelay = argv[4];
char *outDir = argv[5];

int port = 0;
int listenSd = listenOnFreePort(&port);
pid_t server = 0;
if (sameString(mode, "refused"))
    close(listenSd);     /* the port was free a moment ago and nothing listens now */
else if (sameString(mode, "answer"))
    server = startServer(listenSd, TRUE);
else if (sameString(mode, "silent"))
    server = startServer(listenSd, FALSE);
else
    errAbort("unknown mode %s", mode);

char confFile[PATH_LEN], errFile[PATH_LEN];
safef(confFile, sizeof(confFile), "%s/botDelay.%s.%s.%s.%s.conf", outDir, mode, gate, timeout,
    failureDelay);
safef(errFile, sizeof(errFile), "%s/botDelay.%s.%s.%s.%s.err", outDir, mode, gate, timeout,
    failureDelay);
writeConf(confFile, port, gate, timeout, failureDelay);
setenv("HGDB_CONF", confFile, 1);
setenv("REMOTE_ADDR", "192.0.2.7", 1);

printf("%s, serveOnFailure=%s, timeoutSeconds=%s, failureDelay=%s:\n",
    mode, gate, timeout, failureDelay);
fflush(stdout);
if (freopen(errFile, "w", stderr) == NULL)
    errnoAbort("freopen %s", errFile);

time_t start = time(NULL);
boolean failed = FALSE;
int delay = -1;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    delay = hgBotDelayTimeFracWithStatus(1.0, &failed);
errCatchEnd(errCatch);
long elapsed = time(NULL) - start;
fflush(stderr);

if (server > 0)
    {
    if (sameString(mode, "silent"))
        kill(server, SIGTERM);
    waitpid(server, NULL, 0);
    }
if (sameString(mode, "silent"))
    {
    int limit = atoi(timeout);
    if (limit < 1 || limit > 10)
        limit = 10;
    /* At least the timeout, and well short of the server's 30 seconds. */
    printf("  returned after the timeout and before the server let go: %s\n",
        (elapsed >= limit - 1 && elapsed < limit + 10) ? "yes" : "no");
    }
if (errCatch->gotError)
    printf("  aborted: %s\n", trimSpaces(errCatch->message->string));
else
    printf("  delay %d, server failed %s\n", delay, failed ? "yes" : "no");
errCatchFree(&errCatch);
printLog(errFile, port);
return 0;
}
