/* dataSim - Simulate system where data is dynamically distributed. */
#include "paraCommon.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dataSim - Simulate system where data is dynamically distributed\n"
  "usage:\n"
  "   dataSim machine aCount bCount\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

int now;	/* Current time. */

struct fileSize
/* Table of file names and sizes. */
   {
   struct fileSize *next;	/* Next in list. */
   char *name;			/* Name of file. */
   int size;			/* Size of file. */
   };

struct inFile
/* A reference to a file. */
    {
    struct inFile *next;	/* Next in list */
    struct fileSize *file;	/* File. */
    bool isStaged;		/* True if file is staged for this job. */
    };

struct machine
/* Info on a machine. */
    {
    struct machine *next;	/* Next in list. */
    struct dlNode *node;	/* Node on doubly-linked list. */
    char *name;			/* Machine name */
    struct hash *files;		/* Hash of files in machine */
    struct slRef *fileList;	/* List of files in machine. */
    struct job *job;		/* Job on machine. */
    bool isServer;		/* True if won't run jobs. */
    bool busy;			/* Machine is busy. */
    };
struct machine *machineList = NULL;

struct dlList *machFree;	/* Free list. */
struct dlList *machBusy;	/* Machines running jobs. */
struct dlList *machStaging;	/* Machines loading data. */
struct dlList *serverFree;	/* Free server list. */

struct job  
/* Info on a job. */
    {
    struct job *next;	/* Next in main list. */
    struct dlNode *node;	/* Node on doubly-linked list. */
    char *command;	/* Command line. */
    struct inFile *input; /* List of input files. */
    struct machine *machine;	/* Machine if any this is running on. */
    int estTime;	/* Estimated run time. */
    bool isStaged;	/* Jobs input is in place. */
    };
struct job *jobList = NULL;

struct fileMarket
/* A place where files get exchanged. */
    {
    struct fileMarket *next;	/* Next in list. */
    struct fileSize *file;	/* File we're shopping for. */
    struct slRef *demand;	/* Machines that want this file. */
    struct slRef *supply;	/* Machines that have this file. */
    };

struct dlList *jobRunning;   /* Jobs running */
struct dlList *jobWaiting;   /* Jobs waiting */

struct message
/* A message */
    {
    struct message *next;	/* Next in list. */
    struct dlNode *node;	/* Node on doubly-linked list. */
    int time;			/* Message sent time. */
    char *type;			/* Message type. */
    struct machine *m1;		/* First machine involved. */
    struct machine *m2;		/* Second machien involved, may be null. */
    struct fileSize *file;	/* File involved, may be null. */
    struct job *job;		/* Job involved, may be null. */
    };

struct dlList *messTodo;	/* List of messages to process. */
struct dlList *messDone;	/* List of messages processed. */

int jobCmp(const void *va, const void *vb)
/* Compare to sort based on size of job . */
{
const struct job *a = *((struct job **)va);
const struct job *b = *((struct job **)vb);
return b->estTime - a->estTime;
}

void addJob(struct job *job)
/* Add job to list, with largest job first. */
{
struct dlNode *node;
struct job *oj;
boolean added = FALSE;
for (node = jobWaiting->head; !dlEnd(node); node = node->next)
    {
    oj = node->val;
    if (oj->estTime < job->estTime)
        {
	dlAddBefore(node, job->node);
	added = TRUE;
	break;
	}
    }
if (!added)
    dlAddTail(jobWaiting, job->node);
}

void addMessage(int time, char *type, 
	struct machine *m1, struct machine *m2, 
	struct fileSize *file, struct job *job)
/* Add message to queue. */
{
struct dlNode *node;
struct message *om;
struct message *mess;
boolean added = FALSE;

/* Make up message */
AllocVar(mess);
AllocVar(mess->node);
mess->node->val = mess;
mess->time = time;
mess->type = type;
mess->m1 = m1;
mess->m2 = m2;
mess->file = file;
mess->job = job;

/* Put it in queue where it belongs. */
for (node = messTodo->head; !dlEnd(node); node = node->next)
    {
    om = node->val;
    if (mess->time < om->time)
        {
	dlAddBefore(node, mess->node);
	added = TRUE;
	break;
	}
    }
if (!added)
    dlAddTail(messTodo, mess->node);
}

int inFileCmp(const void *va, const void *vb)
/* Compare to sort based on size of file . */
{
const struct inFile *a = *((struct inFile **)va);
const struct inFile *b = *((struct inFile **)vb);
return b->file->size - a->file->size;
}

void dumpJob(struct job *job)
/* Print info on one job. */
{
struct inFile *in;
printf("'%s' %d ", job->command, job->estTime);
for (in = job->input; in != NULL; in = in->next)
    printf("{'%s',%d},", in->file->name, in->file->size);
printf("\n");
}

void dumpJobList(struct dlList *list)
/* Dump out a list of jobs. */
{
struct dlNode *node;
for (node = list->head; !dlEnd(node); node = node->next)
    dumpJob(node->val);
}

void dumpJobs()
/* Print out job list */
{
printf("Running:\n");
dumpJobList(jobRunning);
printf("Waiting:\n");
dumpJobList(jobWaiting);
}

void dumpMachine(struct machine *mach)
/* Dump machine state. */
{
struct hashEl *el, *list = hashElListHash(mach->files);
struct fileSize *file;

printf("%s: ", mach->name);
for (el = list; el != NULL; el = el->next)
    {
    file = el->val;
    printf("%s,", file->name);
    }
if (mach->job)
    printf(" %s", mach->job->command);
printf("\n");
hashElFreeList(&list);
}

void dumpMachines()
/* Print info about machines. */
{
struct machine *mach;
for (mach = machineList; mach != NULL; mach = mach->next)
    dumpMachine(mach);
}

int totalLocalDiskUsage(struct machine *mach)
/* Calculate size of all files on local disk. */
{
struct hashEl *el, *list = hashElListHash(mach->files);
struct fileSize *file;
int total = 0;

for (el = list; el != NULL; el = el->next)
    {
    file = el->val;
    total += file->size;
    }
hashElFreeList(&list);
return total;
}

void printLocalDiskStats()
/* Figure out how much is on local machine disks. */
{
struct machine *mach;
int serverSize = 0, oneSize, maxSize = 0, total = 0;
int machCount = 0;
for (mach = machineList; mach != NULL; mach = mach->next)
    {
    oneSize = totalLocalDiskUsage(mach);
    if (mach->isServer)
        serverSize = oneSize;
    else
	{
	if (oneSize > maxSize)  maxSize = oneSize;
	total += oneSize;
	++machCount;
	}
    }
printf("Total local disk space used: %d\n", total);
printf("Max local disk space used: %d\n", maxSize);
printf("Average local disk space used: %d\n", total/machCount);
printf("Local space on server: %d\n", serverSize);
}

double totalTime(struct job *jobList)
/* REturn total time. */
{
struct job *job;
double total = 0;
for (job = jobList; job != NULL; job = job->next)
    {
    total += job->estTime;
    }
return total;
}

void markIfStaged(struct job *job)
/* Mark files already on machine as staged. */
{
struct machine *mach = job->machine;
boolean isStaged = TRUE;
struct inFile *in;
for (in = job->input; in != NULL; in = in->next)
    {
    if (hashLookup(mach->files, in->file->name))
       in->isStaged = TRUE;
    else
       isStaged = FALSE;
    }
job->isStaged = isStaged;
}

long calcLocal(struct job *job, struct machine *mach)
/* Figure out size of local files needed by job already on machine */
{
struct hash *hash = mach->files;
struct inFile *in;
long total  = 0;
for (in = job->input; in != NULL; in = in->next)
    {
    if (hashLookup(hash, in->file->name))
        total += in->file->size;
    }
return total;
}

struct dlNode *matchJob(struct machine *mach, int maxSearch)
/* Look through top bits of job list and return one that has most
 * local data. */
{
struct dlNode *node, *bestNode = NULL;
long curLocal, bestLocal = -1;
int i=0;

for (node = jobWaiting->head; !dlEnd(node); node = node->next)
    {
    if (++i >= maxSearch)
        break;
    curLocal = calcLocal(node->val, mach);
    if (curLocal > bestLocal)
        {
	bestLocal = curLocal;
	bestNode = node;
	}
    }
return bestNode;
}

boolean claimJobs()
/* Claim a job for ourselves. */
{
struct dlNode *jobNode, *machNode;
struct job *job;
struct machine *mach;
boolean claimed = FALSE;
while (!dlEmpty(jobWaiting) && !dlEmpty(machFree))
     {
     machNode = dlPopHead(machFree);
     mach = machNode->val;
     jobNode = matchJob(mach, 50);
     if (jobNode == NULL)
         warn("Null job node even though %d items in jobWiaiting", dlCount(jobWaiting));
     assert(jobNode != NULL);
     dlRemove(jobNode);
     job = jobNode->val;
     job->machine = mach;
     mach->job = job;
     dlAddTail(jobRunning, jobNode);
     dlAddTail(machStaging, machNode);
     claimed = TRUE;
     markIfStaged(job);
     printf("%d claim %s '%s'\n", now, mach->name, job->command);
     }
return claimed;
}

void makeCopyJob(struct dlNode *source, struct dlNode *dest, struct fileSize *file)
/* Make up job to copy file */
{
struct machine *s = source->val, *d = dest->val;
printf("%d copy %s(%d) from %s to %s\n", now, file->name, file->size, s->name, d->name);
dlRemove(source);
dlRemove(dest);
dlAddTail(machBusy, source);
dlAddTail(machBusy, dest);
addMessage(now + file->size, "copyDone", s, d, file, NULL);
}

void makeRunJob(struct dlNode *machNode, struct job *job)
/* Make up job to run command on node. */
{
struct machine *mach = machNode->val;
dlRemove(machNode);
dlAddTail(machBusy, machNode);
printf("%d run '%s' on %s\n", now, job->command, mach->name);
addMessage(now + job->estTime, "jobDone", mach, NULL, NULL, job);
}


int fileMarketCmp(const void *va, const void *vb)
/* Compare to sort based on size of file in market. */
{
const struct fileMarket *a = *((struct fileMarket **)va);
const struct fileMarket *b = *((struct fileMarket **)vb);
return b->file->size - a->file->size;
}

boolean marketDay(struct fileMarket *marketList)
/* Try and match up supplies and demands. Return TRUE if succeed anywhere */
{
boolean anyMet = FALSE;
struct fileMarket *market;
for (market = marketList; market != NULL; market = market->next)
    {
    struct slRef *rSupply, *rDemand;
    struct machine *supply, *demand;
    int demandCount = slCount(market->demand);
    int demandsMet = 0;
    for (rSupply = market->supply; rSupply != NULL; rSupply = rSupply->next)
         {
	 boolean metOne = FALSE;
	 supply = rSupply->val;
	 if (!supply->busy)
	     {
	     for (rDemand = market->demand; rDemand != NULL; rDemand = rDemand->next)
		 {
		 demand = rDemand->val;
		 if (!demand->busy)
		     {
		     makeCopyJob(supply->node, demand->node, market->file);
		     supply->busy = demand->busy = TRUE;
		     metOne = TRUE;
		     break;
		     }
		 }
	     }
	 if (metOne)
	     {
	     anyMet = TRUE;
	     if (++demandsMet == demandCount)
	         break;
	     }
	 }
    }
return anyMet;
}

void addSupply(struct hash *marketHash, struct dlList *machList)
/* Add supplies from machines to markets. */
{
struct dlNode *node;
int supplyCount = 0;
for (node = machList->head;  !dlEnd(node); node = node->next)
    {
    struct machine *mach = node->val;
    struct slRef *ref;
    for (ref = mach->fileList; ref != NULL; ref = ref->next)
        {
	struct fileSize *file = ref->val;
	struct fileMarket *market;
	if ((market = hashFindVal(marketHash, file->name)) != NULL)
	    {
	    refAdd(&market->supply, mach);
	    ++supplyCount;
	    }
	}
    mach->busy = FALSE;
    }
}

boolean stageJobs()
/* Copy data to set up runs. */
{
boolean staged = FALSE;
struct dlNode *node, *source;
struct fileSize *file;
struct fileMarket *marketList = NULL, *market;
struct hash *marketHash = newHash(9);
struct inFile *in;
int demandCount = 0;

/* Build up markets and  list of demands. */
for (node = machStaging->head; !dlEnd(node); node = node->next)
    {
    struct machine *mach = node->val;
    struct job *job = mach->job;
    if (!job->isStaged)
	{
	for (in = job->input; in != NULL; in = in->next)
	    {
	    if (!in->isStaged)
	        {
		struct fileSize *file = in->file;
		if ((market = hashFindVal(marketHash, file->name)) == NULL)
		    {
		    AllocVar(market);
		    market->file = file;
		    slAddHead(&marketList, market);
		    hashAdd(marketHash, file->name, market);
		    ++demandCount;
		    }
		refAdd(&market->demand, mach);
		}
	    }
	}
    }
slReverse(&marketList);
for (market = marketList; market != NULL; market = market->next)
    slReverse(&market->demand);

/* Add supply from other staging area machines and run market. */
addSupply(marketHash, machStaging);
for (market = marketList; market != NULL; market = market->next)
    slReverse(&market->supply);
staged |= marketDay(marketList);

/* Replace supplies with those from server and run market. */
for (market = marketList; market != NULL; market = market->next)
    slFreeList(&market->supply);
addSupply(marketHash, serverFree);
for (market = marketList; market != NULL; market = market->next)
    slReverse(&market->supply);
staged |= marketDay(marketList);

/* Clean up markets. */
for (market = marketList; market != NULL; market = market->next)
    {
    slFreeList(&market->supply);
    slFreeList(&market->demand);
    }
slFreeList(&marketList);
freeHash(&marketHash);

return staged;
}

boolean runJobs()
/* Run any jobs that are all staged. */
{
struct dlNode *node, *next;
boolean run = FALSE;
for (node = machStaging->head; !dlEnd(node); node = next)
    {
    struct machine *mach = node->val;
    struct job *job = mach->job;
    next = node->next;
    if (job->isStaged)
	{
	makeRunJob(node, job);
	run = TRUE;
	}
    }
return run;
}

void stageAgain(struct machine *mach)
/* Put machine back on free queue. */
{
if (mach != NULL)
   {
   struct dlList *list = (mach->isServer ? serverFree : machStaging);
   struct dlNode *node = mach->node;
   dlRemove(node);
   dlAddHead(list, node);
   }
}

boolean eatMessage()
/* Handle next thing if any on message queue. */
{
struct dlNode *node = dlPopHead(messTodo);
if (node != NULL)
    {
    struct message *mess = node->val;
    struct machine *mach = mess->m1;
    now = mess->time;
    printf("%d %s ", now, mess->type);
    if (sameString(mess->type, "jobDone"))
        {
	printf("'%s' on %s\n", mess->job->command, mach->name);
	mach->job = NULL;
	dlRemove(mach->node);
	dlAddHead(machFree, mach->node);
	}
    else if (sameString(mess->type, "copyDone"))
        {
	struct machine *dest = mess->m2;
	struct fileSize *file = mess->file;
	printf("%s from %s to %s\n", 
		file->name, mess->m1->name, dest->name);
	hashAdd(dest->files, file->name, file);
	refAdd(&dest->fileList, file);
	markIfStaged(dest->job);
	stageAgain(mess->m1);
	stageAgain(dest);
	}
    else
        {
	errAbort("Unknown message type %s", mess->type);
	}
    dlAddTail(messDone, node);
    return TRUE;
    }
else
    return FALSE;
}

void simulate()
/* Run simulation printing what's up. */
{
boolean gotWork = TRUE;
while (gotWork)
    {
    gotWork = FALSE;
    gotWork |= claimJobs();
    gotWork |= stageJobs();
    gotWork |= runJobs();
    gotWork |= eatMessage();
    }
}

void dataSim(int machineCount, int aCount, int bCount)
/* dataSim - Simulate system where data is dynamically distributed. */
{
static int hmDim[2];
int dim, i, j;
struct hash *fileHash = newHash(0);
struct fileSize *fs;
struct job *job;
struct machine *machine;
struct inFile *in;
struct slRef *fileRefList = NULL;
struct dlNode *node;

hmDim[0] = aCount;
hmDim[1] = bCount;
if (machineCount == 0 || aCount == 0 || bCount == 0)
    usage();

/* Dummy up files. */
for (dim=0; dim < ArraySize(hmDim); ++dim)
    {
    char c = 'a' + dim;
    for (i=1; i<=hmDim[dim]; ++i)
	{
	char buf[64];
	sprintf(buf, "%c%02d", c, i);
	AllocVar(fs);
	fs->size = i;
	hashAddSaveName(fileHash, buf, fs, &fs->name);
	refAdd(&fileRefList, fs);
	}
    }

/* Dummy up machines */
machFree = newDlList();
machBusy = newDlList();
machStaging = newDlList();
serverFree = newDlList();
for (i=0; i<=machineCount; ++i)
    {
    char name[64];
    sprintf(name, "m%02d", i);
    AllocVar(machine);
    machine->name = cloneString(name);
    AllocVar(machine->node);
    machine->node->val = machine;
    slAddHead(&machineList, machine);
    if (i == 0)	/* File server is machine 0. */
       {
       machine->files = fileHash;
       machine->fileList = fileRefList;
       machine->isServer = TRUE;
       dlAddTail(serverFree, machine->node);
       }
    else
       {
       machine->files = newHash(11);
       dlAddTail(machFree, machine->node);
       }
    }
slReverse(&machineList);

/* Dummy up jobs. */
jobRunning = newDlList();
jobWaiting = newDlList();
for (i = 1; i <= hmDim[0]; ++i)
    {
    char aName[64];
    char command[256];
    sprintf(aName, "a%02d", i);
    for (j=1; j <= hmDim[1]; ++j)
        {
	char bName[64];
	sprintf(bName, "b%02d", j);
	AllocVar(job);
	sprintf(command, "blat %s %s %s_%s.psl", aName, bName, aName, bName);
	job->command = cloneString(command);
	AllocVar(in);
	in->file = hashMustFindVal(fileHash, aName);
	slAddHead(&job->input, in);
	AllocVar(in);
	in->file = hashMustFindVal(fileHash, bName);
	slAddHead(&job->input, in);
	job->estTime = i*j+1;
	slAddHead(&jobList, job);
	AllocVar(job->node);
	job->node->val = job;
	dlAddTail(jobWaiting, job->node);
	}
    }
dlSort(jobWaiting, jobCmp);


/* Swap file order in every-other job.  This mixing tends to
 * let us get started on the binary file copy better. */
i = 0;
for (node = jobWaiting->head; !dlEnd(node); node = node->next)
   {
   if (++i & 1)
       {
       job = node->val;
       slReverse(&job->input);
       }
   }


/* Start message lists. */
messTodo = newDlList();
messDone = newDlList();

/* Do simulation. */
simulate();

dumpMachines();
printLocalDiskStats();
printf("total jobs: %d\n", slCount(jobList));
printf("cpu time: %0.1f\n", totalTime(jobList));
printf("wall clock time: %d\n", now);
printf("parallel efficiency: %f\n", (totalTime(jobList)/machineCount)/now);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
dataSim(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
return 0;
}
