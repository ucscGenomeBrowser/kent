/* broadNode - Daemon that runs on cluster nodes in broadcast data system. */
#include "paraCommon.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "log.h"
#include "paraLib.h"
#include "broadData.h"
#include "md5.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"hubInPort", OPTION_INT},
    {"nodeInPort", OPTION_INT},
    {"logFacility", OPTION_STRING},
    {"logMinPriority", OPTION_STRING},
    {"log", OPTION_STRING},
    {"drop", OPTION_INT},
    {"ip", OPTION_STRING},
    {"broadIp", OPTION_STRING},
    {NULL, 0}
};

char *broadIp = "10.1.255.255";

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
  "   -logFacility=facility log to the specified syslog facility.\n"
  "   -logMinPriority=pri minimum syslog priority to log, also filters file logging.\n"
  "   -log=file  log to file instead of syslog.\n"
  "   -debug  Don't daemonize\n"
  "   -drop=N - Drop every Nth packet\n"
  "   -ip=NNN.NNN.NNN.NNN ip address of current machine, usually needed.\n"
  "   -broadIp - network broadcast address, %s by default\n"
  , bdHubInPort, bdNodeInPort, broadIp
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
sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
if (sd < 0)
    errAbort("Couldn't open datagram socket");
if (bind(sd, (struct sockaddr *)&sai, sizeof(sai)) < 0)
    errAbort("Couldn't bind socket");
if ((err = getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, &sizeSize)) != 0)
    errAbort("Couldn't get socket size");
return sd;
}

int openOutputSocket(int port)
{
int sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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
ret = ntohl(ret);
return ret;
}

bits32 getOwnIpAddress()
/* Return IP address of ourselves. */
{
static bits32 id = 0;
if (id == 0)
    {
    char *machName = optionVal("ip", NULL);
    if (machName == NULL) 
       errAbort("Need to specify ip address with -ip");
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
   int curSectionIx;	/* Which section are we working on? */
   int curSectionSize;	/* Size of current section. */
   struct fileSection section; /* Keep track of blocks in this section. */
   char sectionData[bdSectionBytes]; /* Data for this section. */
   bool sectionClosed;	/* True if this section is closed. */
   unsigned char md5[16];	/* Keep the md5 checksum here. */
   bool doneMd5;	/* Calculated md5. */
   };

void fileTrackerFree(struct fileTracker **pFt)
/* Free up file tracker */
{
struct fileTracker *ft = *pFt;
if (ft != NULL)
    {
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
    ft->sectionClosed = TRUE;
    ft->curSectionIx = -1;
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
    struct fileSection *section = &ft->section;
    off_t startOffset;
    if (ft->curSectionIx != sectionIx)
	{
	if (!ft->sectionClosed)
	    {
	    logDebug("Moving on with an unclosed section %d in %s", 
                     ft->curSectionIx, ft->fileName);
	    return -222;
	    }
	ft->curSectionIx = sectionIx;
	ft->curSectionSize = 0;
	ft->sectionClosed = FALSE;
	ft->doneMd5 = FALSE;
	memset(section->blockTracker, 0, sizeof(section->blockTracker));
	}
    if (section->blockTracker[blockIx] == FALSE)
	{
	int dataOffset = blockIx * bdBlockSize;
	int dataEnd = dataOffset + size;
	startOffset = bdBlockOffset(sectionIx, blockIx);
	if (ft->pos != startOffset)
	    {
	    if (lseek(ft->fh, startOffset, SEEK_SET) == -1)
		return errno;
	    ft->pos = startOffset;
	    }
	if (dataEnd > ft->curSectionSize) ft->curSectionSize = dataEnd;
	memcpy(ft->sectionData + dataOffset, data, size);
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

void calcMd5OnSection(struct fileTracker *ft)
/* Calculate md5. */
{
if (!ft->doneMd5)
    {
#ifdef SOON
    md5_starts(&ctx);
    md5_update(&ctx, (uint8 *)ft->sectionData, ft->curSectionSize);
    md5_finish(&ctx, ft->md5);
#endif
    ft->doneMd5 = TRUE;
    }
}

void pSectionDone(struct bdMessage *m, struct fileTracker *ftList, bits32 ownIp)
/* Try to get ahead on md5 count. */
{
struct fileTracker *ft;
bits32 fileId, sectionIx, blockCount, i;
bdParseSectionDoneMessage(m, &fileId, &sectionIx, &blockCount);
ft = findTracker(ftList, fileId);
if (ft != NULL && sectionIx == ft->curSectionIx)
    {
    int missingCount = 0;
    for (i=0; i<blockCount; ++i)
	++missingCount;
    if (missingCount == 0)
	calcMd5OnSection(ft);
    }
}

void pSectionQuery(struct bdMessage *m, struct fileTracker *ftList, bits32 ownIp)
/* Process section query.  Set up message so that it contains list
 * of missing blocks. */
{
struct fileTracker *ft;
bits32 fileId, sectionIx, blockCount;
unsigned char *md5;
bdParseSectionQueryMessage(m, &fileId, &sectionIx, &blockCount, &md5);
ft = findTracker(ftList, fileId);
if (ft == NULL || sectionIx < ft->curSectionIx)
    {
    /* Don't complain about missing stuff.  Supposively at least
     * we've already dealt with this, and this is just a late message
     * coming into the socket. */
    bdMakeMissingBlocksMessage(m, ownIp, m->id, fileId, 0, NULL);
    }
else
    {
    /* Go through section and store missing blockIx's. */
    struct fileSection *section = &ft->section;
    bits32 *missingList = ((bits32 *)m->data) + 2;
    bits32 *ml = missingList;
    int i, missingCount = 0;
    if (sectionIx > ft->curSectionIx)
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
	for (i=0; i<blockCount; ++i)
	    {
	    if (!section->blockTracker[i])
		{
		*ml++ = i;
		++missingCount;
		}
	    }
	}
    logDebug("%d missing %d of %d", sectionIx, missingCount, blockCount);
    if (missingCount == 0)
	{
	/* Check md5 signature.  If it matches we're good to go, otherwise
	 * set up things to tell server to resend the whole section. */
#ifdef SOON
	calcMd5OnSection(ft);
	if (memcmp(ft->md5, md5, sizeof(ft->md5)) == 0)
	    ft->sectionClosed = TRUE;
	else
	    {
	    logDebug("Section %d of %s failed md5 check", sectionIx, ft->fileName);
	    for (i=0; i<blockCount; ++i)
		{
		section->blockTracker[i] = FALSE;
		*ml++ = i;
		++missingCount;
		}
	    ft->doneMd5 = FALSE;
	    }
#else /* SOON */
	ft->sectionClosed = TRUE;
#endif /* SOON */
	}
    bdMakeMissingBlocksMessage(m, ownIp, m->id, fileId, missingCount, missingList);
    }
}

void broadNode()
/* broadNode - Daemon that runs on cluster nodes in broadcast data system. */
{
int inSd, outSd;
int err = 0;
struct bdMessage *m = NULL;
boolean alive = TRUE;
bits32 ownIp = getOwnIpAddress();
bits32 sourceIp;
struct fileTracker *ftList = NULL;
int drop = 0;

inSd = openInputSocket(nodeInPort);
outSd = openOutputSocket(hubInPort);
AllocVar(m);
while (alive)
    {
    findNow();
    if ((err = bdReceive(inSd, m, &sourceIp)) < 0)
	warn("bdReceive error %s", strerror(err));
    else
	{
	// logDebug("got message type %d size %d sourceIp %x", m->type, m->size, sourceIp);
	if (dropTest != 0)
	    {
	    if (--drop <= 0)
		{
		drop = dropTest;
		continue;
		}
	    }
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
		if (m->machine == ownIp)
		    err = pFileClose(m, &ftList);
		personallyAck(outSd, m, ownIp, sourceIp, hubInPort, err);
		break;
	    case bdmPing:
		// logDebug("<PING>%s</PING>", m->data);
		personallyAck(outSd, m, ownIp, sourceIp, hubInPort, 0);
		break;
	    case bdmSectionQuery:
		if (m->machine == ownIp)
		    {
		    pSectionQuery(m, ftList, ownIp);
		    bdSendTo(outSd, m, sourceIp, hubInPort);
		    }
		break;
	    case bdmMissingBlocks:
		break;
	    case bdmSectionDone:
	        break;
	    default:
		break;
	    }
	}
    }
close(inSd);
close(outSd);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 2)
    usage();
nodeInPort = optionInt("nodeInPort", bdNodeInPort);
hubInPort = optionInt("hubInPort", bdHubInPort);
broadIp = optionVal("broadIp", broadIp);
dropTest = optionInt("drop", 0);
paraDaemonize("broadNode");
broadNode();
return 0;
}
