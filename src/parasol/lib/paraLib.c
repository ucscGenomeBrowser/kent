#include "paraCommon.h"
#include <sys/utsname.h>
#include "options.h"
#include "errabort.h"
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

int paraHubPort = 0x46DC;		      /* Our hub port */
int paraNodePort = 0x46DD;		      /* Our node port */

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

char* paraFormatIp(bits32 ip)
/* format a binary IP added into dotted quad format.  ip should be
 * in host byte order. Warning: static return. */
{
static char dottedQuad[32];
if (!internetIpToDottedQuad(ip, dottedQuad))
    safef(dottedQuad, sizeof(dottedQuad), "<invalid-ip:0x%x>", ip);
return dottedQuad;
}

void paraDaemonize(char *progName)
/* daemonize parasol server process, closing open file descriptors and
 * starting logging based on the -logFacility and -log command line options .
 * if -debug is supplied , don't fork. */
{
logDaemonize(progName);
}

