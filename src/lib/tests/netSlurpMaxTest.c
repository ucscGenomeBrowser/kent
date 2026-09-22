/* netSlurpMaxTest - check the ceiling on a response read into memory, added for #38359.
 *
 * hgTablesTest runs under pushCarefulMemHandler(), and that ceiling is enforced by
 * exit(1) from inside carefulAlloc, deliberately not by errAbort, because errAbort
 * itself allocates.  A dense file-backed track answered a five megabyte test region
 * with 602MB, the allocator exited where it stood, and the run ended with one line
 * on stderr, nothing in the log, and every table still to come forfeited.
 *
 * The fix stops the read before the allocator is asked for memory it will refuse.
 * netSlurpFileMax() gives up and returns NULL past a caller-set size, and htmlPage
 * turns that into an errAbort the robot's errCatch can catch, so the run names the
 * track and carries on.
 *
 * None of this shows on a page, so a unit test is the only test there can be.
 * Four parts:
 *   1. netSlurpFileMax on a plain file, both sides of the boundary.
 *   2. The same call under pushCarefulMemHandler, checking that the buffer it
 *      abandons is freed rather than left counted against the ceiling for the
 *      rest of the run.
 *   3. A matched pair of forked children: uncapped, the read still dies in the
 *      allocator, which is the failure the cap exists to prevent; capped, the
 *      same read on the same file returns nothing and the process lives.
 *   4. htmlPageSetMaxSize() wired through htmlSlurpWithCookies(), against a
 *      server on the loopback interface, because that is the path hgTablesTest
 *      actually takes and the ceiling reaches it through a module-wide static.
 *
 * refs #38359 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "dystring.h"
#include "errAbort.h"
#include "errCatch.h"
#include "htmlPage.h"
#include "memalloc.h"
#include "net.h"
#include "obscure.h"
#include "verbose.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define DATA_FILE "output/netSlurpMax.dat"
#define EMPTY_FILE "output/netSlurpMaxEmpty.dat"
#define BIG_FILE "output/netSlurpMaxBig.dat"

#define DATA_SIZE 10000         /* more than two of the 4kb reads inside netSlurpFileMax */
#define BIG_SIZE 3000000        /* comfortably past CAREFUL_CEILING once the buffer doubles */
#define CAREFUL_CEILING 1000000 /* stands in for the robot's 500MB, scaled down */
#define BODY_SIZE 20000         /* body of the canned http response */

static int errCount = 0;

static void expect(boolean ok, char *label)
/* Print one result line, and remember a failure for the exit status. */
{
printf("%-58s %s\n", label, ok ? "ok" : "FAIL");
if (!ok)
    ++errCount;
}

static char *patternBuf(int size)
/* Return a zero terminated buffer of the given size, filled with a repeating
 * pattern so that a short or shifted read shows up as a mismatch rather than
 * matching by luck. */
{
char *buf = needLargeMem(size+1);
int i;
for (i=0; i<size; ++i)
    buf[i] = 'a' + (i % 26);
buf[size] = 0;
return buf;
}

static struct dyString *slurpFile(char *fileName, size_t maxSize)
/* Open fileName and read it through netSlurpFileMax at the given ceiling. */
{
int fd = mustOpenFd(fileName, O_RDONLY);
struct dyString *dy = netSlurpFileMax(fd, maxSize);
close(fd);
return dy;
}

static void fileCases(char *want)
/* Read a file of known contents at ceilings either side of its size.  The
 * boundary is the part worth pinning: the ceiling is a limit on what may be
 * read, so a file exactly that size has to come back whole. */
{
struct
    {
    size_t max;
    boolean wantData;
    char *label;
    } cases[] = {
    {0,             TRUE,  "no ceiling reads the whole file"},
    {DATA_SIZE+1,   TRUE,  "ceiling above the file reads it"},
    {DATA_SIZE,     TRUE,  "ceiling exactly at the file size reads it"},
    {DATA_SIZE-1,   FALSE, "ceiling one byte under returns nothing"},
    {1,             FALSE, "ceiling under the very first read returns nothing"},
    };
int i;
for (i=0; i<ArraySize(cases); ++i)
    {
    struct dyString *dy = slurpFile(DATA_FILE, cases[i].max);
    boolean ok;
    if (cases[i].wantData)
        ok = (dy != NULL && dy->stringSize == DATA_SIZE
              && memcmp(dy->string, want, DATA_SIZE) == 0);
    else
        ok = (dy == NULL);
    expect(ok, cases[i].label);
    dyStringFree(&dy);
    }

/* An empty file is not an oversized one, however low the ceiling. */
struct dyString *dy = slurpFile(EMPTY_FILE, 1);
expect(dy != NULL && dy->stringSize == 0, "an empty file is read, not refused");
dyStringFree(&dy);
}

static void carefulCases()
/* The same reads under the allocator hgTablesTest installs.  What matters here
 * is the accounting: a buffer abandoned on the way out would stay counted
 * against the ceiling for the rest of the run, so the cap would buy one
 * oversized page and no more. */
{
pushCarefulMemHandler(CAREFUL_CEILING);
size_t before = carefulTotalAllocated();

struct dyString *dy = slurpFile(DATA_FILE, DATA_SIZE-1);
expect(dy == NULL, "over the ceiling under carefulAlloc, nothing comes back");
expect(carefulTotalAllocated() == before, "the abandoned buffer is freed, not left counted");

dy = slurpFile(DATA_FILE, 0);
expect(dy != NULL && carefulTotalAllocated() > before, "a page that is read is still held");
dyStringFree(&dy);
expect(carefulTotalAllocated() == before, "and is released when the caller frees it");

/* Repeating the refused read must not creep: ten of them leave the total where
 * one did. */
int i;
for (i=0; i<10; ++i)
    {
    dy = slurpFile(DATA_FILE, DATA_SIZE-1);
    dyStringFree(&dy);
    }
expect(carefulTotalAllocated() == before, "ten refused reads in a row leak nothing");

popMemHandler();
}

static int childSlurps(char *fileName, size_t maxSize)
/* Read fileName at the given ceiling in a child process under
 * pushCarefulMemHandler, and return the child's exit status.  A child is the
 * only way to watch this: carefulAlloc answers the ceiling with exit(1), which
 * no errCatch can see, and that is the whole complaint in the ticket. */
{
fflush(stdout);
pid_t pid = fork();
if (pid < 0)
    errnoAbort("netSlurpMaxTest: cannot fork");
if (pid == 0)
    {
    /* carefulAlloc writes its complaint straight to fd 2, so silence the fd
     * rather than stderr, and silence stdout too since exit(1) will flush
     * whatever this process inherited in the buffer. */
    int devNull = open("/dev/null", O_WRONLY);
    if (devNull >= 0)
        {
        dup2(devNull, STDOUT_FILENO);
        dup2(devNull, STDERR_FILENO);
        }
    pushCarefulMemHandler(CAREFUL_CEILING);
    struct dyString *dy = slurpFile(fileName, maxSize);
    _exit(dy == NULL ? 2 : 0);
    }
int status = 0;
if (waitpid(pid, &status, 0) < 0)
    errnoAbort("netSlurpMaxTest: cannot wait for child");
return status;
}

static void allocatorCases()
/* The pair that says what the cap is for.  Same file, same allocator ceiling,
 * one read capped and one not. */
{
int status = childSlurps(BIG_FILE, 0);
expect(WIFEXITED(status) && WEXITSTATUS(status) == 1,
       "uncapped, the allocator still exits the process");

status = childSlurps(BIG_FILE, CAREFUL_CEILING/5);
expect(WIFEXITED(status) && WEXITSTATUS(status) == 2,
       "capped, the same read returns nothing and the process lives");
}

static int listenOnLoopback(int *retPort)
/* Listen on 127.0.0.1 on a port the kernel picks, and return the socket.
 * Letting the kernel pick keeps this from colliding with anything else on the
 * build machine. */
{
int sd = socket(AF_INET, SOCK_STREAM, 0);
if (sd < 0)
    errnoAbort("netSlurpMaxTest: cannot make a socket");
struct sockaddr_in sai;
ZeroVar(&sai);
sai.sin_family = AF_INET;
sai.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
sai.sin_port = 0;
if (bind(sd, (struct sockaddr *)&sai, sizeof(sai)) < 0)
    errnoAbort("netSlurpMaxTest: cannot bind to the loopback interface");
if (listen(sd, 8) < 0)
    errnoAbort("netSlurpMaxTest: cannot listen");
socklen_t saiSize = sizeof(sai);
if (getsockname(sd, (struct sockaddr *)&sai, &saiSize) < 0)
    errnoAbort("netSlurpMaxTest: cannot read back the port");
*retPort = ntohs(sai.sin_port);
return sd;
}

static void serveUntilKilled(int listenSd, char *response, int responseSize)
/* Answer every connection with the same bytes.  Never returns; the parent kills
 * it.  A caller that gives up part way through leaves this writing into a closed
 * socket, which is one of the cases under test, so SIGPIPE has to go. */
{
signal(SIGPIPE, SIG_IGN);
for (;;)
    {
    int sd = accept(listenSd, NULL, NULL);
    if (sd < 0)
        _exit(1);
    char request[4096];
    ssize_t got = read(sd, request, sizeof(request));
    ssize_t put = write(sd, response, responseSize);
    verbose(3, "served a request of %lld bytes, wrote %lld\n", (long long)got, (long long)put);
    close(sd);
    }
}

static char *fetchOrAbortMessage(char *url)
/* Fetch url through htmlSlurpWithCookies.  Return NULL if it came back, or the
 * abort message if it did not.  The caller frees the message. */
{
char *message = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    char *text = htmlSlurpWithCookies(url, NULL);
    if (text == NULL || strlen(text) != 0)
        verbose(3, "fetched %d bytes\n", text == NULL ? -1 : (int)strlen(text));
    freez(&text);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    message = cloneString(errCatch->message->string);
errCatchFree(&errCatch);
return message;
}

static int fetchSize(char *url)
/* Fetch url and return how many bytes came back, or -1 if it aborted. */
{
int size = -1;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    char *text = htmlSlurpWithCookies(url, NULL);
    size = strlen(text);
    freez(&text);
    }
errCatchEnd(errCatch);
errCatchFree(&errCatch);
return size;
}

static void htmlPageCases()
/* htmlPageSetMaxSize reaching the fetch that hgTablesTest uses.  The ceiling is
 * a module-wide static set from main(), so the wiring between the setter and
 * the two fetch sites is the thing that can rot without any caller changing. */
{
struct dyString *dy = dyStringNew(0);
char *body = patternBuf(BODY_SIZE);
dyStringPrintf(dy, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n",
               BODY_SIZE);
dyStringAppendN(dy, body, BODY_SIZE);
freez(&body);
int responseSize = dy->stringSize;

int port = 0;
int listenSd = listenOnLoopback(&port);
fflush(stdout);
pid_t pid = fork();
if (pid < 0)
    errnoAbort("netSlurpMaxTest: cannot fork the loopback server");
if (pid == 0)
    serveUntilKilled(listenSd, dy->string, responseSize);
close(listenSd);

/* No handler: the default action for SIGALRM ends the process, which is what
 * should happen if the server or a fetch wedges.  A hung test that make waits
 * on forever is worse than a loud one. */
alarm(60);

char url[256];
safef(url, sizeof(url), "http://127.0.0.1:%d/anything", port);

htmlPageSetMaxSize(0);
expect(fetchSize(url) == responseSize, "no ceiling fetches the whole response");

htmlPageSetMaxSize(responseSize);
expect(fetchSize(url) == responseSize, "ceiling exactly at the response size fetches it");

htmlPageSetMaxSize(responseSize-1);
char *message = fetchOrAbortMessage(url);
expect(message != NULL && startsWith(HTML_PAGE_TOO_BIG, message),
       "ceiling one byte under aborts with the message the robot reads");
freez(&message);

htmlPageSetMaxSize(1000);
message = fetchOrAbortMessage(url);
expect(message != NULL && startsWith(HTML_PAGE_TOO_BIG, message),
       "a ceiling well under the response aborts the same way");
freez(&message);

/* Back to the default, which is what leaves hgNearTest, hgBlatTest and
 * htmlCheck as they were. */
htmlPageSetMaxSize(0);
expect(fetchSize(url) == responseSize, "the ceiling can be turned back off");

alarm(0);
kill(pid, SIGTERM);
int status = 0;
if (waitpid(pid, &status, 0) < 0)
    errnoAbort("netSlurpMaxTest: cannot wait for the loopback server");
dyStringFree(&dy);
}

int main(int argc, char *argv[])
{
char *want = patternBuf(DATA_SIZE);
writeGulp(DATA_FILE, want, DATA_SIZE);
writeGulp(EMPTY_FILE, want, 0);
char *big = patternBuf(BIG_SIZE);
writeGulp(BIG_FILE, big, BIG_SIZE);
freez(&big);

fileCases(want);
carefulCases();
allocatorCases();
htmlPageCases();

freez(&want);
remove(DATA_FILE);
remove(EMPTY_FILE);
remove(BIG_FILE);
printf("%d failures\n", errCount);
return errCount != 0;
}
