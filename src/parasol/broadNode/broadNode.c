/* broadNode - Daemon that runs on cluster nodes in broadcast data system. */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "paraLib.h"
#include "broadData.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "broadNode - Daemon that runs on cluster nodes in broadcast data system\n"
  "usage:\n"
  "   broadNode now\n"
  "options:\n"
  "   -hubInPort=N (default %d)\n"
  "   -nodeInPort=N (default %d)\n"
  "   -log=fileName - where to write log messages\n"
  "   -drop=N - Drop every Nth packet\n"
  "   -ip=NNN.NNN.NNN.NNN ip address of current machine, usually needed.\n"
  , bdHubInPort, bdNodeInPort
  );
}

int hubInPort, nodeInPort;
int dropTest = 0;

int openInputSocket(int port)
/* Open up a datagram socket that can accept messages from anywhere. */
{
struct sockaddr_in sai;
int err, sd;
int size, sizeSize = sizeof(size);

ZeroVar(&sai);
sai.sin_family = AF_INET;
sai.sin_port = htons(port);
sai.sin_addr.s_addr = INADDR_ANY;
sd = socket(AF_INET, SOCK_DGRAM, 0);
if (sd < 0)
    errAbort("Couldn't open datagram socket");
if (bind(sd, (struct sockaddr *)&sai, sizeof(sai)) < 0)
    errAbort("Couldn't bind socket");
if ((err = getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, &sizeSize)) != 0)
    errAbort("Couldn't get socket size");
if (size < 32000)
    {
    size = 64*1024;
    if ((err = setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size))) != 0)
	errAbort("Couldn't set socket buffer to %d", size);
    }
return sd;
}

int openOutputSocket()
{
int sd = socket(AF_INET, SOCK_DGRAM, 0);
if (sd < 0)
    errAbort("Couldn't open datagram socket");
return sd;
}

bits32 ipForName(char *name)
/* Return named IP address. */
{
struct hostent *hostent;
bits32 ret;
hostent = gethostbyname(name);
if (hostent == NULL)
    errAbort("Couldn't find host %s. h_errno %d", name, h_errno);
    {
    int i,j;
    for (i=0; ; ++i)
        {
	char *addr = hostent->h_addr_list[i];

	if (addr == NULL)
	    break;
	for (j=0; j<4; ++j)
	    {
	    printf("%d", addr[j]);
	    putchar( (j == 3 ? '\n' : '.'));
	    }
	}
    }

memcpy(&ret, hostent->h_addr_list[0], sizeof(ret));
return ret;
}

bits32 getOwnIpAddress()
/* Return IP address of ourselves. */
{
static bits32 id = 0;
if (id == 0)
    {
    char *machName = optionVal("ip", NULL);
    if (machName == NULL) machName = getMachine();
    id = ipForName(machName);
    }
return id;
}

void personallyAck(int sd, struct bdMessage *m, bits32 ownIp, bits32 destIp, 
	int port, int err)
/* Send acknowledgement message if message targets this node personally. */
{
if (m->machine == ownIp)
    {
    bdMakeAckMessage(m, ownIp, m->id, err);
    bdSendTo(sd, m, destIp, port);
    }
}

struct fileSection
/* Keep track of a section of a file. */
   {
   bool blockTracker[bdSectionBlocks];	/* Set to TRUE when have seen block. */
   };

struct fileTracker
/* Information on a file. */
   {
   struct fileTracker *next;	/* Next in list. */
   bits32 fileId;	/* File id number. */
   char *fileName;	/* File name. */
   int fh;		/* Open file handle. */
   time_t openTime;	/* Time file opened. */
   off_t pos;		/* Position in file. */
   int sectionAlloc;	/* Size of sections array. */
   struct fileSection **sections;	/* Array of sections. */
   };

void fileTrackerFree(struct fileTracker **pFt)
/* Free up file tracker */
{
struct fileTracker *ft = *pFt;
if (ft != NULL)
    {
    int i;
    for (i=0; i<ft->sectionAlloc; ++i)
	freeMem(ft->sections[i]);
    freeMem(ft->sections);
    freeMem(ft->fileName);
    freez(pFt);
    }
}

struct fileTracker *findTracker(struct fileTracker *list, bits32 fileId)
/* Find file tracker on list. */
{
struct fileTracker *ft;
for (ft = list; ft != NULL; ft = ft->next)
    {
    if (ft->fileId == fileId)
	return ft;
    }
return NULL;
}

int pFileOpen(struct bdMessage *m, struct fileTracker **pList)
/* Process file open message.  Create a new file tracker if we
 * don't have one already.  Return 0 on success, error number on
 * failure. */
{
struct fileTracker *ft;
bits32 fileId;
char *fileName;

bdParseFileOpenMessage(m, &fileId, &fileName);
ft = findTracker(*pList, fileId);
if (ft == NULL)
    {
    int fh = creat(fileName, 0777);
    if (fh < 0)
	return fh;
    AllocVar(ft);
    ft->fileId = fileId;
    ft->fileName = cloneString(fileName);
    ft->fh = fh;
    ft->openTime = time(NULL);
    ft->sectionAlloc = 1;
    AllocArray(ft->sections, ft->sectionAlloc);
    slAddHead(pList, ft);
    }
return 0;
}

int pBlock(struct bdMessage *m, struct fileTracker *list)
/* Process block message.  Write out data.  Keep track of block. */
{
bits32 fileId, sectionIx, blockIx;
int size;
void *data;
struct fileTracker *ft;
bdParseBlockMessage(m, &fileId, &sectionIx, &blockIx, &size, &data);
assert(blockIx <= bdSectionBlocks);
if (size == 0)
    return 0;
ft = findTracker(list, fileId);
if (ft != NULL)
    {
    struct fileSection *section;
    off_t startOffset;
    if (sectionIx >= ft->sectionAlloc)
	{
	int newAlloc = ft->sectionAlloc*2;
	ExpandArray(ft->sections, ft->sectionAlloc, newAlloc);
	ft->sectionAlloc = newAlloc;
	}
    section = ft->sections[sectionIx];
    if (section == NULL)
	{
	AllocVar(section);
	ft->sections[sectionIx] = section;
	}
    if (section->blockTracker[blockIx] == FALSE)
	{
	startOffset = bdBlockOffset(sectionIx, blockIx);
	if (ft->pos != startOffset)
	    {
	    if (lseek(ft->fh, startOffset, SEEK_SET) == -1)
		return errno;
	    ft->pos = startOffset;
	    }
	size = write(ft->fh, data, size);
	if (size == -1)
	    return errno;
	ft->pos += size;
	section->blockTracker[blockIx] = TRUE;
	}
    }
return 0;
}

int pFileClose(struct bdMessage *m, struct fileTracker **pList)
/* Close file and free associated resources. */
{
bits32 fileId;
char *fileName;
struct fileTracker *ft;
bdParseFileCloseMessage(m, &fileId, &fileName);
ft = findTracker(*pList, fileId);
if (ft != NULL)
    {
    boolean sameName = sameString(ft->fileName, fileName);
    int err = close(ft->fh);
    slRemoveEl(pList, ft);
    fileTrackerFree(&ft);
    if (err < 0)
	return errno;
    if (!sameName)
	return -111;
    }
return 0;
}

void pSectionQuery(struct bdMessage *m, struct fileTracker *ftList, bits32 ownIp)
/* Process section query.  Set up message so that it contains list
 * of missing blocks. */
{
struct fileTracker *ft;
bits32 fileId, sectionIx, blockCount;
bdParseSectionQueryMessage(m, &fileId, &sectionIx, &blockCount);
ft = findTracker(ftList, fileId);
if (ft == NULL)
    {
    /* We aren't even supposed to know about this file, so
     * don't complain about missing stuff. */
    bdMakeMissingBlocksMessage(m, ownIp, m->id, fileId, 0, NULL);
    }
else
    {
    /* Go through section and store missing blockIx's. */
    bits32 *missingList = ((bits32 *)m->data) + 2;
    bits32 *ml = missingList;
    int i, missingCount = 0;
    if (sectionIx >= ft->sectionAlloc || ft->sections[sectionIx] == NULL)
	{
	/* We haven't even seen anything from this section.  Return it all
	 * as missing. */
	for (i=0; i<blockCount; ++i)
	    {
	    *ml++ = i;
	    ++missingCount;
	    }
	}
    else
	{
	struct fileSection *section = ft->sections[sectionIx];
	for (i=0; i<blockCount; ++i)
	    {
	    if (!section->blockTracker[i])
		{
		*ml++ = i;
		++missingCount;
		}
	    }
	}
    logIt("%d missing %d of %d\n", sectionIx, missingCount, blockCount);
    bdMakeMissingBlocksMessage(m, ownIp, m->id, fileId, missingCount, missingList);
    }
}

void broadNode()
/* broadNode - Daemon that runs on cluster nodes in broadcast data system. */
{
int port = 0;
int inSd, outSd;
int err = 0;
struct bdMessage *m = NULL;
boolean alive = TRUE;
bits32 ownIp = getOwnIpAddress();
bits32 sourceIp;
struct fileTracker *ftList = NULL;
int drop = 0;

inSd = openInputSocket(nodeInPort);
outSd = openOutputSocket();
AllocVar(m);
while (alive)
    {
    if ((err = bdReceive(inSd, m, &sourceIp)) < 0)
	warn("bdReceive error %s", strerror(err));
    else
	{
	// logIt("got message type %d size %d sourceIp %x\n", m->type, m->size, sourceIp);
	if (dropTest != 0)
	    {
	    if (--drop <= 0)
		{
		drop = dropTest;
		continue;
		}
	    }
	m->machine = ownIp;
	switch (m->type)
	    {
	    case bdmQuit:
		alive = FALSE;
		personallyAck(outSd, m, ownIp, sourceIp, hubInPort, 0);
		break;
	    case bdmFileOpen:
		err = pFileOpen(m, &ftList);
		personallyAck(outSd, m, ownIp, sourceIp, hubInPort, err);
		break;
	    case bdmBlock:
		err = pBlock(m, ftList);
		personallyAck(outSd, m, ownIp, sourceIp, hubInPort, err);
		break;
	    case bdmFileClose:
		err = pFileClose(m, &ftList);
		personallyAck(outSd, m, ownIp, sourceIp, hubInPort, err);
		break;
	    case bdmPing:
		logIt("<PING>\n%s</PING>\n", m->data);
		personallyAck(outSd, m, ownIp, sourceIp, hubInPort, 0);
		break;
	    case bdmSectionQuery:
		pSectionQuery(m, ftList, ownIp);
		bdSendTo(outSd, m, sourceIp, hubInPort);
		break;
	    case bdmMissingBlocks:
		break;
	    default:
		break;
	    }
	}
    }
close(inSd);
close(outSd);
}

void forkDaemon()
/* Fork off real daemon and exit.  This effectively
 * removes dependence of paraNode daemon on terminal. 
 * Set up log file if any here as well. */
{
char *log = optionVal("log", NULL);

/* Close standard file handles. */
close(0);
if (log == NULL || !sameString(log, "stdout"))
    close(1);
close(2);

if (fork() == 0)
    {
    /* Set up log handler. */
    setupDaemonLog(log);
    logFlush = TRUE;

    /* Execute daemon. */
    broadNode();
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
nodeInPort = optionInt("nodeInPort", bdNodeInPort);
hubInPort = optionInt("hubInPort", bdHubInPort);
dropTest = optionInt("drop", 0);
forkDaemon();
return 0;
}
