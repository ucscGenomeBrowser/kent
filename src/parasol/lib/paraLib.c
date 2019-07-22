#include "paraCommon.h"
#include <sys/utsname.h>
#include "options.h"
#include "errAbort.h"
#include "net.h"
#include "portable.h"
#include "internet.h"
#include "log.h"
#include "sqlNum.h"
#include "paraLib.h"

time_t now;	/* Time when started processing current message */

void findNow()
/* Just set now to current time. */
{
now = time(NULL);
}

char paraSig[17] = "0d2f070562685f29";  /* Mild security measure. */
int paraSigSize = 16;

char *paraHubPortStr  = "18140";      /* Our hub  port as string */
char *paraNodePortStr = "18141";      /* Our node port as string */

char *getMachine()
/* Return host name. */
{
static char *host = NULL;
if (host == NULL)
    {
    host = getenv("HOST");
    if (host == NULL)
	{
	struct utsname unamebuf;
	if (uname(&unamebuf) < 0)
	    errAbort("Couldn't find HOST environment variable or good uname");
	host = unamebuf.nodename;
	}
    host = cloneString(host);
    }
return host;
}

int forkOrDie()
/* Fork, aborting if it fails. */
{
int childId = mustFork();
return childId;
}

boolean parseRunJobMessage(char *line, struct runJobMessage *rjm)
/* Parse runJobMessage as paraNodes sees it. */
{
bool ret;
rjm->jobIdString = nextWord(&line);
rjm->reserved = nextWord(&line);
rjm->user = nextWord(&line);
rjm->dir = nextWord(&line);
rjm->in = nextWord(&line);
rjm->out = nextWord(&line);
rjm->err = nextWord(&line);
rjm->cpus = sqlFloat(nextWord(&line));
rjm->ram = sqlLongLong(nextWord(&line));
rjm->command = line = skipLeadingSpaces(line);
ret = (line != NULL && line[0] != 0);
if (!ret)
    warn("Badly formatted jobMessage");
return ret;
}

void fillInErrFile(char errFile[512], int jobId, char *tempDir )
/* Fill in error file name */
{
sprintf(errFile, "%s/para%d.err", tempDir, jobId);
}

char* paraFormatIp(struct in6_addr * ip)
/* format a binary IP added into dotted quad format.  ip should be
 * in host byte order. Warning: static return. */
{
static char ipStr[INET6_ADDRSTRLEN];
if (inet_ntop(AF_INET6, ip, ipStr, sizeof(ipStr)) < 0)
    {
    char tempHex[33];
    ip6AddrToHexStr(ip, tempHex, sizeof tempHex);
    safef(ipStr, sizeof(ipStr), "<invalid-ip:0x%s>", tempHex);
    }
return ipStr;
}

void paraDaemonize(char *progName)
/* daemonize parasol server process, closing open file descriptors and
 * starting logging based on the -logFacility and -log command line options .
 * if -debug is supplied , don't fork. */
{
logDaemonize(progName);
}

long long paraParseRam(char *ram)
/* Parse RAM expression like 2000000, 2t, 2g, 2m, 2k
 * Returns long long number of bytes, or -1 for error
 * The value of input variable ram may be modified. */
{
long long result = -1, factor = 1;
int len = strlen(ram);
int i;
char saveC = ' ';
if (len == 0)
    return result;
if (ram[len-1] == 't')
    factor = (long long)1024 * 1024 * 1024 * 1024;
else if (ram[len-1] == 'g')
    factor = 1024 * 1024 * 1024;
else if (ram[len-1] == 'm')
    factor = 1024 * 1024;
else if (ram[len-1] == 'k')
    factor = 1024;
if (factor != 1)
    {
    --len;
    saveC = ram[len];
    ram[len] = 0;
    }
for (i=0; i<len; ++i)
    if (!isdigit(ram[i]))
	return result;
result = factor * sqlLongLong(ram);
if (factor != 1)
    ram[len] = saveC;
return result;
}

