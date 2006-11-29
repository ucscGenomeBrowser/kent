/* broadHub - Hub for file broadcast system. */
#include "paraCommon.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
#include "net.h"
#include "obscure.h"
#include "md5.h"
#include "broadData.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"hubInPort", OPTION_INT},
    {"nodeInPort", OPTION_INT},
    {"broadIp", OPTION_STRING},
    {"verbose", OPTION_INT},
    {"test", OPTION_INT},
    {NULL, 0}
};

char *broadIp = "10.1.255.255";
int initTimeOut = 50000;
bits32 broadAddr;
int verbosity = 2;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "broadHub - Hub for file broadcast system\n"
  "usage:\n"
  "   broadHub machine.lst fileName\n"
  "options:\n"
  "   -hubInPort=N (default %d)\n"
  "   -nodeInPort=N (default %d)\n"
  "   -broadIp - network broadcast address, %s by default.\n"
  "   -verbose=N Level of verbosity, default %d. 0 is silent.\n"
  , bdHubInPort, bdNodeInPort, broadIp, verbosity
  );
}

void verbage(int level, char *format, ...)
/* Print out verbage if the level less than or equal to the
 * current verbosity leve. */
{
if (level <= verbosity)
    {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    }
}

int hubInPort, nodeInPort;

int broadcast(int sd, struct bdMessage *m)
/* Send broadcast message. */
{
return bdSendTo(sd, m, broadAddr, nodeInPort);
}

void setReceiveTimeOut(int socket, int timeOut)
/* Set timeout value for socket. */
{
struct timeval tv;
int tvSize = sizeof(tv);
int err;
tv.tv_sec = timeOut/1000000;
tv.tv_usec = timeOut%1000000;
err = setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, tvSize);
if (err < 0)
    errAbort("Can't set timeout on socket");
}

int openHubInSocket(int port, int timeOut)
/* Open up a datagram socket that can accept messages from anywhere, and
 * that can times out. */
{
struct sockaddr_in sai;
int sd;

ZeroVar(&sai);
sai.sin_family = AF_INET;
sai.sin_port = htons(port);
sai.sin_addr.s_addr = INADDR_ANY;
sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
if (bind(sd, (struct sockaddr *)&sai, sizeof(sai)) < 0)
    errAbort("Couldn't bind socket");
setReceiveTimeOut(sd, timeOut);
return sd;
}

void setBroadcast(int sd)
/* Turn on/off broadcast on socket. */
{
int ttl = 1;
int on = 1;
/* Set socket to enable broadcast. */
if (setsockopt(sd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) != 0)
    errAbort("Can't set broadcast option on socket %s", strerror(errno));
/* Packets shouldn't live outside of local network. */
if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) != 0)
    errAbort("Can't set ttl option on socket %s", strerror(errno));
}

int openBroadcastSocket(int port)
/* Open up a datagram socket that can broadcast. */
{
int sd;

/* Make sure header shortcuts don't trip us up. */
assert(sizeof(struct bdMessage) == bdHeaderSize + bdMaxDataSize);

/* Open datagram socket to broadcast . */
sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
if (sd < 0)
    errAbort("Can't open broadcast socket %s", strerror(sd));
setBroadcast(sd);

return sd;
}

long microTime()
/* Return time in microseconds. */
{
static boolean initted = FALSE;
static int origSec;
struct timeval tv;
gettimeofday(&tv, NULL);
if (!initted)
    {
    initted = TRUE;
    origSec = tv.tv_sec;
    }
return (tv.tv_sec - origSec)*1000000 + tv.tv_usec;
}

struct machine
/* Keep track of one machine. */
    {
    char *name;	  /* Machine name. */
    bits32 ip;    /* IP Address. */
    int blockErrCount; /* Number of block errors. */
    int timeOutCount;  /* Number of time outs. */
    int timeOutTime;   /* Time out time. */
    boolean isDead;    /* Set if machine looks dead. */
    boolean didOpen;   /* Did open ok. */
    boolean didClose;  /* Did close ok. */
    boolean gotCleanStatus;	/* Did status ok. */
    };

struct dlList *getMachines(char *fileName)
/* Get machine list from file and connect to it. */
{
char *row[1], *name;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct machine *machine;
struct dlList *list = newDlList();
struct hostent *hostent;
struct hash *uniqHash = newHash(0); /* Make sure IP addresses are unique. */
char ascIp[24];	/* Ip address as ascii. */

while (lineFileRow(lf, row))
    {
    name = row[0];
    AllocVar(machine);
    machine->name = cloneString(name);
    if (isdigit(name[0]) && countChars(name, '.') == 3  && strlen(name) < 16)
	{
	/* If it's a dotted quad extract the ip address 
	 * without involving DNS. */
	machine->ip = ntohl(inet_addr(name));
	}
    else
	{
	/* Go ping the DNS.  This could take a while... */
	hostent = gethostbyname(name);
	if (hostent == NULL)
	    {
	    warn("Couldn't find host %s. h_errno %d", name, h_errno);
	    continue;
	    }
	memcpy(&machine->ip, hostent->h_addr_list[0], sizeof(bits32));
	}
    sprintf(ascIp, "%x", machine->ip);
    if (hashLookup(uniqHash, ascIp))
	errAbort("Duplicated IP address %s line %d of %s", name, lf->lineIx, lf->fileName);
    hashAdd(uniqHash, ascIp, NULL);
    dlAddValTail(list, machine);
    }
hashFree(&uniqHash);
return list;
}

struct machine *findMachine(struct dlList *machineList, bits32 ip)
/* Find machine of given IP address on list. */
{
struct dlNode *mNode;
struct machine *machine;

for (mNode = machineList->head; !dlEnd(mNode); mNode = mNode->next)
    {
    machine = mNode->val;
    if (machine->ip == ip)
	return machine;
    }
return NULL;
}

struct machine *nextLivingMachine(struct dlList *machineList, struct dlList *deadList)
/* Return next machine on list that is not dead, rotating list in process
 * and weeding out dead machines.  Returns NULL if nothing alive. */
{
struct dlNode *mNode;
struct machine *machine;

while ((mNode = dlPopHead(machineList)) != NULL)
    {
    machine = mNode->val;
    if (machine->isDead)
	dlAddTail(deadList, mNode);
    else
	{
	dlAddTail(machineList, mNode);
	return machine;
	}
    }
return NULL;
}

void trackTimeOuts(int err, struct machine *machine, int *pTimeOutCount)
/* Adjust time outs downwards on success and upwards on error.
 * Update time out count if need be on machine. */
{
if (err < 0)
    {
    machine->timeOutCount += 1;
    *pTimeOutCount += 1;
    }
}

void receiveAndIgnore(int inSd, struct bdMessage *m, int count)
/* Throw count packets into the bit bucket, stopping as soon as
 * the socket is empty. */
{
int i, err;
bits32 ip;
for (i=0; i<count; ++i)
    {
    err = bdReceive(inSd, m, &ip);
    if (err < 0)
	break;
    }
}

void sendFile(struct dlList *machineList, struct dlList *deadList, 
	struct bdMessage *m, int inSd, int outSd, char *fileName)
/* Broadcast file. */
{
struct dlNode *mNode, *nextNode;
struct machine *machine;
int tryIx, maxTry = 4, maxBroadTry = maxTry*10;
int fileId = 0;
int messageIx = 0;
int firstOpenMessage = messageIx + 1;
int firstCloseMessage;
bits32 ip;
char *fileDataArea = m->data + 3 * sizeof(bits32);
int readSize;
int firstCheckMessage, rescueCount = 0;
int sectionIx, subIx, subBlockIx, blockIx, blockCount, subBlockCount = 0;
int err;
FILE *f;
boolean allDone;
int statBlocks = 0, statResent = 0;
int totalTimeOuts = 0;
struct md5_context ctx;
unsigned char md5sum[16];
long t1,t2, openTime =0, closeTime = 0, sendTime = 0,
	primaryTime = 0, checkTime = 0;

/* Open up file. */
f = mustOpen(fileName, "rb");
// if (fd < 0)
if (f == NULL)
    errnoAbort("Can't open %s", fileName);

/* Tell nodes things are coming twice by broadcast and then one at a time. */
t1 = microTime();
bdMakeFileOpenMessage(m, 0, ++messageIx, fileId, fileName);
err = broadcast(outSd, m);
err = broadcast(outSd, m);
for (tryIx = 0; tryIx < maxTry; ++tryIx)
    {
    for (mNode = machineList->head; !dlEnd(mNode); mNode = mNode->next)
	{
	machine = mNode->val;
	if (!machine->didOpen && !machine->isDead)
	    {
	    int err;
	    bdMakeFileOpenMessage(m, machine->ip, ++messageIx, fileId, fileName);
	    err = bdSendTo(outSd, m, machine->ip, nodeInPort);
	    if ((err = bdReceive(inSd, m, &ip)) >= 0)
		{
		if (m->id >= firstOpenMessage)
		    {
		    machine = findMachine(machineList, ip);
		    if (machine != NULL)
			{
			signed32 err;
			bdParseAckMessage(m, &err);
			if (err < 0)
			    {
			    verbage(1, "%s: %s %s\n", machine->name, fileName, strerror(err));
			    machine->isDead = TRUE;
			    }
			else
			    {
			    machine->didOpen = TRUE;
			    }
			}
		    else
		        {
			verbage(1, "Couldn't find machine %x\n", ip);
			}
		    }
		}
	    trackTimeOuts(err, machine, &totalTimeOuts);
	    }
	}
    }


/* Mark as dead machines that did not open successfully. */
for (mNode = machineList->head; !dlEnd(mNode); mNode = nextNode)
    {
    nextNode = mNode->next;
    machine = mNode->val;
    if (machine->didOpen && !machine->isDead)
	 {
	 }
     else
	 {
	 verbage(1, "%s could not open file\n", machine->name);
	 machine->isDead = TRUE;
	 dlRemove(mNode);
	 dlAddTail(deadList, mNode);
	 }
    }
openTime = microTime() - t1;

if (dlCount(machineList) <=  0)
    {
    warn("All machines seem to be dead\n");
    }
else
    {
    /* Do each section. */
    t1 = microTime();
    allDone = FALSE;
    for (sectionIx = 0; !allDone;  ++sectionIx)
	{
	int subStart = 0;
	/* Seek to section start (error recovery might have moved us) */
	off_t offset = bdBlockOffset(sectionIx, 0);
	blockCount = 0;
	// if (lseek(fd, offset, SEEK_SET) == -1)
	if (fseek(f, offset, SEEK_SET) == -1)
	    errnoAbort("Couldn't seek in %s", fileName);

	/* Do primary broadcast for section. */
	verbage(2, "Section %d\n", sectionIx);
	md5_starts(&ctx);
	t2 = microTime();
	for (subIx=0,subStart=0; subIx < bdSubSectionCount && !allDone; ++subIx, subStart += bdSubSectionSize)
	    {
	    subBlockCount = 0;
	    blockIx = subStart;
	    for (subBlockIx = 0; subBlockIx < bdSubSectionSize; ++subBlockIx, ++blockIx)
		{
		machine = nextLivingMachine(machineList, deadList);
		if (machine == NULL)
		   errAbort("All machines seem to be dead now.");
		// readSize = netReadAll(fd, fileDataArea, bdBlockSize);
		readSize = fread(fileDataArea, 1, bdBlockSize, f);
		++statBlocks;
		if (readSize < 0)
		    errAbort("Read error on host %s, file %s", machine->name, fileName);
		md5_update(&ctx, (uint8 *)fileDataArea, readSize);
		bdMakeBlockMessage(m, machine->ip, ++messageIx, fileId, sectionIx, 
		    blockIx, readSize, fileDataArea);
		broadcast(outSd, m);
		++subBlockCount;
		++blockCount;
		if (readSize < bdBlockSize)
		    {
		    allDone = TRUE;
		    verbage(1, "all done with %s!\n", fileName);
		    break;
		    }
		}
	    blockIx = subStart;
	    receiveAndIgnore(inSd, m, subBlockCount);
	    }
	/* Give nodes heads up so they can start computing md5 and stuff. */
	bdMakeSectionDoneMessage(m, 0, ++messageIx, fileId, sectionIx, blockCount);
	broadcast(outSd, m);
	md5_finish(&ctx, md5sum);
	primaryTime += microTime() - t2;

	/* Check nodes one at a time for this section. */
	firstCheckMessage = messageIx + 1;
	t2 = microTime();
	for (mNode = machineList->head; !dlEnd(mNode); mNode = mNode->next)
	    {
	    machine = mNode->val;
	    machine->gotCleanStatus = FALSE;
	    }
	for (tryIx = 0; tryIx < maxBroadTry; ++tryIx)
	    {
	    for (mNode = machineList->head; !dlEnd(mNode); mNode = mNode->next)
		{
		machine = mNode->val;
		if (!machine->gotCleanStatus)
		    {
		    boolean gotReply = FALSE;
		    bdMakeSectionQueryMessage(m, machine->ip, ++messageIx, fileId, 
			sectionIx, blockCount, md5sum);
		    bdSendTo(outSd, m, machine->ip, nodeInPort);
		    /* Try to find reply to us, skipping any previous messages. */
		    for (;;)
			{
			if ((err = bdReceive(inSd, m, &ip)) < 0)
			    {
			    verbage(2, " %s timed out checking\n", machine->name);
			    trackTimeOuts(err, machine, &totalTimeOuts);
			    break;
			    }
			if (m->type == bdmMissingBlocks && m->machine == machine->ip && m->id >= firstCheckMessage)
			    {
			    if (m->id != messageIx)
				{
			        ++rescueCount;
				verbage(2, " rescued a missingBlockMessage on %s\n", machine->name);
				}
			    gotReply = TRUE;
			    break;
			    }
			}
		    if (gotReply)
			{
			bits32 fileId, missingCount, *missingList;
			bdParseMissingBlocksMessage(m, &fileId, &missingCount, &missingList);
			if (missingCount == 0)
			    machine->gotCleanStatus = TRUE;
			else
			    {
			    int i, ackPending = 0;
			    struct bdMessage *m2;  /* Need 2nd message, still using first. */
			    char *dataArea2;
			    AllocVar(m2);
			    dataArea2 = m2->data + 3 * sizeof(bits32);
			    verbage(2, "%d missing blocks from %s\n", missingCount, machine->name);
			    for (i=0; i<missingCount; ++i)
				{
				bits32 blockIx = missingList[i];
				off_t offset = bdBlockOffset(sectionIx, blockIx);
				// if (lseek(fd, offset, SEEK_SET) == -1)
				if (fseek(f, offset, SEEK_SET) == -1)
				    errnoAbort("Couldn't seek in %s", fileName);
				//readSize = netReadAll(fd, dataArea2, bdBlockSize);
				readSize = fread(dataArea2, 1, bdBlockSize, f);
				bdMakeBlockMessage(m2, ip, ++messageIx, fileId, sectionIx, 
					blockIx, readSize, dataArea2);
				broadcast(outSd, m2);
				if (++ackPending >= bdSubSectionSize)
				    {
				    receiveAndIgnore(inSd, m2, ackPending);
				    ackPending = 0;
				    }
				trackTimeOuts(err, machine, &totalTimeOuts);
				++statResent;
				}
			    receiveAndIgnore(inSd, m2, ackPending);
			    freez(&m2);
			    }
			}
		    }
		}
	    }

	/* Mark nodes that did not come through clean as dead. */
	for (mNode = machineList->head; !dlEnd(mNode); mNode = nextNode)
	    {
	    nextNode = mNode->next;
	    machine = mNode->val;
	    if (machine->gotCleanStatus)
		{
		}
	    else
		{
		verbage(1, "%s is unclean\n", machine->name);
		machine->isDead = TRUE;
		dlRemove(mNode);
		dlAddTail(deadList, mNode);
		}
	    }
	checkTime += microTime() - t2;
	}
    sendTime = microTime() - t1;    

    /* Go through each node closing files. */
    t1 = microTime();
    firstCloseMessage = messageIx + 1;
    for (tryIx = 0; tryIx < maxTry; ++tryIx)
	{
	for (mNode = machineList->head; !dlEnd(mNode); mNode = mNode->next)
	    {
	    machine = mNode->val;
	    if (!machine->didClose && !machine->isDead)
		{
		boolean gotReply = FALSE;
		bdMakeFileCloseMessage(m, machine->ip, ++messageIx, fileId, fileName);
		bdSendTo(outSd, m, machine->ip, nodeInPort);
		/* Get message, skipping over non-closing ones. */
		for (;;)
		    {
		    if (bdReceive(inSd, m, &ip) < 0)
			break;
		    if (m->id >= firstCloseMessage)
			{
			gotReply = TRUE;
			break;
			}
		    }
		if (gotReply)
		    {
		    machine = findMachine(machineList, ip);
		    if (machine != NULL)
			{
			signed32 err;
			bdParseAckMessage(m, &err);
			if (err < 0)
			    {
			    verbage(1, "%s: %s %s", machine->name, fileName, strerror(err));
			    machine->isDead = TRUE;
			    }
			else
			    {
			    machine->didClose = TRUE;
			    }
			}
		    }
		}
	    }
	}

    /* Mark as dead machines that did not close successfully. */
    for (mNode = machineList->head; !dlEnd(mNode); mNode = nextNode)
	{
	nextNode = mNode->next;
	machine = mNode->val;
	if (machine->didClose && !machine->isDead)
	    {
	    }
	else
	    {
	    verbage(1, "%s is dead on close\n", machine->name);
	    machine->isDead = TRUE;
	    dlRemove(mNode);
	    dlAddTail(deadList, mNode);
	    }
	}
    closeTime = microTime() - t1;
    verbage(1, "%d blocks, %d (%4.2f%%) resent, %d time outs\n", statBlocks, statResent, 100.0 * statResent/statBlocks, totalTimeOuts);
    verbage(1, "openTime %ld, sendTime %ld, closeTime %ld\n", openTime/1000, sendTime/1000, closeTime/1000);
    verbage(1, "\tprimaryTime %ld, checkTime %ld\n", primaryTime/1000, checkTime/1000);
    verbage(1, "\trescued %d\n", rescueCount);
    }
}


void busyWait(int micros)
/* Tight loop that does nothing. */
{
long destTime = microTime() + micros;
while (microTime() < destTime)
    ;
}

void test2(char *machineFile, char *transferFile)
/* do a little testing. */
{
int i;
int timeCount = 100000;
long t1,t2;
int inSd, outSd, err = 0;
int messageIx = 0;
struct bdMessage *m = NULL;
struct dlList *machineList = getMachines(machineFile);
struct dlList *deadList = newDlList();
struct machine *machine;
int *bTimes, *rTimes, *fTimes;
int errCount = 0;
char buf[1444];
int readSize = sizeof(buf);
int blockCount = 0;
FILE *f = mustOpen(transferFile, "rb");

inSd = openHubInSocket(hubInPort, 500000);
outSd = openBroadcastSocket(nodeInPort);
AllocArray(bTimes, timeCount);
AllocArray(rTimes, timeCount);
AllocArray(fTimes, timeCount);
AllocVar(m);
machine = nextLivingMachine(machineList, deadList);
for (i=0; i<timeCount; ++i)
    {
    t1 = microTime();
    readSize = fread(m->data, 1, sizeof(buf), f);
    t2 = microTime();
    fTimes[i] = t2-t1;
    if (readSize > 0)
	{
	machine = nextLivingMachine(machineList, deadList);
	bdInitMessage(m, machine->ip, ++messageIx, bdmPing, sizeof(buf));
	t1 = microTime();
	broadcast(outSd, m);
	t2 = microTime();
	bTimes[i] = t2-t1;
	t1 = t2;
//	err = bdReceive(inSd, m, &ip);
	t2 = microTime();
	rTimes[i] = t2-t1;
	if (err < 0)
	   ++errCount;
	++blockCount;
	}
    if (readSize < sizeof(buf))
	{
	uglyf("read size %d\n", readSize);
        break;
	}
    }
#ifdef SOON
for (i=0; i<timeCount; ++i)
    {
    printf("%d\t%d\t%d\t%d\n", bTimes[i] + rTimes[i] + fTimes[i], bTimes[i], rTimes[i], fTimes[i]);
    }
#endif /* SOON */
fprintf(stderr, "%d errors in %d\n", errCount, timeCount);
}

void test(char *machineFile, char *transferFile)
/* do a little testing. */
{
int i,j;
int inSd, outSd, err = 0;
int messageIx = 0;
bits32 ip;
struct bdMessage *m = NULL;
struct dlList *machineList = getMachines(machineFile);
struct dlList *deadList = newDlList();
struct machine *machine;
int errCount = 0;

inSd = openHubInSocket(hubInPort, 100000);
outSd = openBroadcastSocket(nodeInPort);
AllocVar(m);
for (j=0; j<510; ++j)
    {
    for (i=0; i<64; ++i)
	{
	machine = nextLivingMachine(machineList, deadList);
	bdInitMessage(m, machine->ip, ++messageIx, bdmPing, 1000);
	broadcast(outSd, m);
	}
    for (i=0; i<64; ++i)
	{
	err = bdReceive(inSd, m, &ip);
	if (err < 0)
	   ++errCount;
	}
    }
uglyf("ErrCount %d\n", errCount);
uglyf("bdBlockSize %d, bdSectionBytes %d\n", bdBlockSize, bdSectionBytes);
bdInitMessage(m, 0, -1, bdmQuit, 0);	/* Send quit for the moment. */
broadcast(outSd, m);
broadcast(outSd, m);

}

int broadHub(char *machineFile, char *transferFile)
/* broadHub - Hub for file broadcast system. */
{
int inSd, outSd;
int err = 0;
struct dlList *machineList = getMachines(machineFile);
struct dlList *deadList = newDlList();
struct dlNode *mNode;
struct machine *machine;
struct bdMessage *m = NULL;


inSd = openHubInSocket(hubInPort, 100000);
outSd = openBroadcastSocket(nodeInPort);
verbage(1, "Got %d machines.  inSd %d, outSd %d\n", dlCount(machineList), inSd, outSd);
AllocVar(m);
sendFile(machineList, deadList, m, inSd, outSd, transferFile);
printf("Successfully copied to %d machines\n", dlCount(machineList));
if (dlCount(deadList) != 0)
    {
    warn("Problems on %d machines:", dlCount(deadList));
    for (mNode = deadList->head; !dlEnd(mNode); mNode = mNode->next)
	{
	machine = mNode->val;
	warn("%s", machine->name);
	}
    err = -1;
    }

bdInitMessage(m, 0, -1, bdmQuit, 0);	/* Send quit for the moment. */
broadcast(outSd, m);
broadcast(outSd, m);

close(outSd);
close(inSd);
return err;
}

int main(int argc, char *argv[])
/* Process command line. */
{
int err = 0; 
optionInit(&argc, argv, optionSpecs);
nodeInPort = optionInt("nodeInPort", bdNodeInPort);
hubInPort = optionInt("hubInPort", bdHubInPort);
broadIp = optionVal("broadIp", broadIp);
broadAddr = ntohl(inet_addr(broadIp));
verbosity = optionInt("verbose", verbosity);
if (argc != 3)
    usage();
if (optionExists("test"))
    test(argv[1], argv[2]);
else
    err = broadHub(argv[1], argv[2]);
return err;
}
