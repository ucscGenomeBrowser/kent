/* ccCp - copy file to the cluster efficiently.  Uses child process
 * tracking mechanism. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "dlist.h"
#include "portable.h"
#include "dystring.h"
#include "options.h"
#include <sys/wait.h>


int crossSwitchMax = 40;		/* Max copies between switches at once. */

void usage()
/* Explain usage and exit. */
{
errAbort(
"ccCp - copy a file to cluster."
"usage:\n"
"   ccCp sourceFile destFile [hostList]\n"
"This will copy sourceFile to destFile for all machines in\n"
"hostList\n"
"options\n"
"    -crossMax=N (default %d) - maximum copies across switches\n"
"example:\n"
"   ccCp h.zip /var/tmp/h.zip newHosts"
	, crossSwitchMax);
}

enum 	/* Constants. */
    {
    maxErrCount = 3,	/* Max number of errors before giving up. */
    };

struct netSwitch
/* Keep track of a network switch. */
    {
    struct netSwitch *next;	/* Next switch. */
    char *name;			/* Switch name. */
    int machCount;              /* Number of machines on this switch. */
    struct machine *machList;	/* The machines this is hooked to. */
    };

struct machine
/* Keep track of one machine in cluster. */
    {
    struct machine *next;
    char *name;		/* Name (like cc08) */
    struct netSwitch *netSwitch;  /* Which switch this is on. */
    int errCount;       /* Number of times have gotten error. */
    int pid;            /* Process ID if copying. */
    int status;         /* Status. */
    struct dlNode *sourceNode; /* Source node in copy. */
    };

int crossSwitchCount = 0;	/* Current copies between switches. */

boolean matchMaker(struct dlList *finList, struct dlList *toDoList, 
	struct dlNode **retSourceNode, struct dlNode **retDestNode)
/* Find a copy to make from the finished list to the toDo list. */
{
struct dlNode *sourceNode, *destNode = NULL;
struct machine *source, *dest;
bool gotIt = FALSE;

for (sourceNode = finList->head; sourceNode->next != NULL; sourceNode = sourceNode->next)
    {
    source = sourceNode->val;
    for (destNode = toDoList->head; destNode->next != NULL; destNode = destNode->next)
	{
	dest = destNode->val;
	if (dest->netSwitch == source->netSwitch)
	    {
	    gotIt = TRUE;
	    break;
	    }
	if (crossSwitchCount < crossSwitchMax)
	    {
	    ++crossSwitchCount;
	    gotIt = TRUE;
	    break;
	    }
	}
    if (gotIt)
	break;
    }
if (!gotIt)
    return FALSE;
dlRemove(sourceNode);
dlRemove(destNode);
*retSourceNode = sourceNode;
*retDestNode = destNode;
//uglyf("Matched (%s %s) and (%s %s) cross %d\n",
//	source->name, source->netSwitch->name,
//	dest->name, dest->netSwitch->name, crossSwitchCount);
return TRUE;
}

struct netSwitch *findSwitch(struct netSwitch *nsList, char *name)
/* Return matching switch or NULL if none. */
{
struct netSwitch *ns;
for (ns = nsList; ns != NULL; ns = ns->next)
    if (sameString(ns->name, name))
	return ns;
return NULL;
}

int netSwitchCmp(const void *va, const void *vb)
/* Compare two netSwitchs to sort with most nodes first. */
{
const struct netSwitch *a = *((struct netSwitch **)va);
const struct netSwitch *b = *((struct netSwitch **)vb);
return b->machCount - a->machCount;
}

char *fullPathName(char *relName)
/* Return full version of path name. */
{
char firstChar = relName[0];
char fullPath[512];
char dir[512];

if (firstChar == '/' || firstChar == '~')
    return cloneString(relName);
if (getcwd(dir, sizeof(dir)) == NULL)
    errAbort("fullPathName: getcwd failed!");
sprintf(fullPath, "%s/%s", dir, relName);
return cloneString(fullPath);
}

struct machine *shuffleSwitches(struct netSwitch *nsList)
/* Return machine list that is shuffled from switch to switch. */
{
struct netSwitch *ns;
struct machine *machList = NULL, *mach;
bool gotAny = TRUE;
int iteration = 0;
int machineIx;

while (gotAny)
    {
    ++iteration;
    machineIx = 0;
    gotAny = FALSE;
    for (ns = nsList; ns != NULL; ns = ns->next)
	{
	++machineIx;
	if (ns->machList != NULL)
	    {
	    mach = ns->machList;
	    ns->machList = mach->next;
	    slAddHead(&machList, mach);
	    gotAny = TRUE;
	    if (machineIx >= iteration)	  /* Seed biggest switches first. */
		break;
	    }
	}
    }
slReverse(&machList);
return machList;
}

void readHosts(char *hostList, struct machine **retMachineList, struct netSwitch **retNsList)
/* Create list of all machines from hostList. */
{
struct lineFile *lf = lineFileOpen(hostList, TRUE);
struct machine *machList = NULL, *mach;
struct netSwitch *nsList = NULL, *ns;
char *line, *words[4];
int lineSize, wordCount;
char *switchName;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
	continue;
    wordCount = chopLine(line, words);
    if (wordCount < 1 || wordCount > 2)
	errAbort("Expecting one or two words line %d of %s", lf->lineIx, lf->fileName);
    AllocVar(mach);
    mach->name = cloneString(words[0]);
    if (wordCount > 1)
	switchName = words[1];
    else
    	switchName = "1";
    if ((ns = findSwitch(nsList, switchName)) == NULL)
	{
	AllocVar(ns);
	ns->name = cloneString(switchName);
	slAddHead(&nsList, ns);
	}
    mach->netSwitch = ns;
    ns->machCount += 1;
    slAddHead(&ns->machList, mach);
    }
for (ns = nsList; ns != NULL; ns = ns->next)
    slReverse(&ns->machList);
slSort(&nsList, netSwitchCmp);
machList = shuffleSwitches(nsList);
*retMachineList = machList;
*retNsList = nsList;
}

void dlAddMiddle(struct dlList *list, struct dlNode *node)
/* Add node to approximate middle of list. */
{
int listSize = dlCount(list);
int midPos = listSize/2;
if (midPos <= 0)
    dlAddTail(list, node);
else
    {
    struct dlNode *ln = list->head;
    int i;
    for (i=0; i<midPos; ++i)
	ln = ln->next;
    dlAddAfter(ln, node);
    }
}

void startCopy(struct machine *source, struct machine *dest, char *file, 
	char *thisHost, struct dyString *cmd)
/* Copy file from source to dest machine by executing rcp on dest, using cmd as
 * place to put the system call command. */
{
int childPid;

dyStringClear(cmd);
if (!sameString(thisHost, dest->name))
    dyStringPrintf(cmd, "rsh %s ", dest->name);
dyStringPrintf(cmd, "rcp %s:%s %s", source->name, file, file);
childPid = fork();
if (childPid)
    {
    dest->pid = childPid;
    }
else
    {
    int err;
    err = system(cmd->string);
    if (err != 0)
	err = -1;
    exit(err);
    }
}

void ccCp(char *source, char *dest, char *hostList)
/* Copy source to dest on all files in hostList. */
{
time_t startTime = time(NULL);
time_t curTime, lastTime = 0;
struct machine *machineList = NULL;
struct netSwitch *nsList;
struct machine *m, *m2;
struct dlList *toDoList = newDlList();          /* We haven't done these. */
struct dlList *finishedList = newDlList();	/* All done here. */
struct dlList *sourceList = newDlList();        /* These are sources for copies. */
struct dlList *workingList = newDlList();       /* These are copying data to themselves. */
struct dlList *errList = newDlList();           /* These are messed up 3x or more. */
struct dlNode *finNode, *node, *sourceNode, *destNode;
struct dyString *cmd = newDyString(256);
int machineCount;
int machinesFinished = 0;
char *thisHost = getenv("HOST");
off_t size;
int goodMachines;
double grandTotal;

/* Get host and switch info. */
readHosts(hostList, &machineList, &nsList);
machineCount = slCount(machineList);

/* Make sure file exists.... */
if (!fileExists(source))
    errAbort("%s doesn't exist\n", source);
size = fileSize(source);
printf("Copying %s (%lld bytes) to %d machines\n", source, (unsigned long long)size, machineCount);

/* Add everything to the to-do list. */
for (m = machineList; m != NULL; m = m->next)
    {
    dlAddValTail(toDoList, m);
    }


/* Loop through to-do list trying to do first copy. */
for (node = toDoList->head; node->next != NULL; node = node->next)
    {
    m = node->val;
    dyStringClear(cmd);
    m = node->val;
    if (sameString(thisHost, m->name))
	{
	if (sameString(source, dest))
	    {
	    /* Hey, this is too easy. */
	    ++machinesFinished;
	    break;
	    }
	else
	    {
	    dyStringPrintf(cmd, "cp %s %s", source, dest);
	    }
	}
    else
	{
	dyStringPrintf(cmd, "rcp %s %s:%s", source, m->name, dest);
	}
    if (system(cmd->string) == 0)
	{
	dlRemove(node);
	dlAddTail(finishedList, node);
	++machinesFinished;
	break;
	}
    else  /* some error in rcp */
	{
	warn("Problem with %s\n", cmd->string);
	m->errCount += 1;
	}
    }

/* Loop around launching child processes to copy and
 * wait for them to finish. */
while (machinesFinished < machineCount)
    {
    int pid;
    int status;

    /* Start all possible copies. */
    while (matchMaker(finishedList, toDoList, &sourceNode, &destNode))
	{
	dlAddTail(sourceList, sourceNode);
	dlAddTail(workingList, destNode);
	m = destNode->val;
	m->sourceNode = sourceNode;
	startCopy(sourceNode->val, destNode->val, dest, thisHost, cmd);
	}

    curTime = time(NULL);
    if (curTime - lastTime >= 3)
	{
	printf("%d finished in %d seconds, %d in progress, %d to start, %d errors, %d total\n",
	    dlCount(finishedList) + dlCount(sourceList), (int)(curTime - startTime),
	    dlCount(workingList), dlCount(toDoList), dlCount(errList), machineCount);
	lastTime = curTime;
	}

    /* Wait for a child to finish.  Figure out which machine it is. */
    pid = wait(&status);
    finNode = NULL;
    for (node = workingList->head; node->next != NULL; node = node->next)
	{
	m = node->val;
	if (m->pid == pid)
	    {
	    finNode = node;
	    break;
	    }
	}
    if (finNode == NULL)
	{
	errAbort("Returned from wait on unknown child %d\n", pid);
	continue;
	}

    m = finNode->val;
    m->pid = 0;
    dlRemove(finNode);
    dlRemove(m->sourceNode);
    m2 = m->sourceNode->val;
    if (m->netSwitch != m2->netSwitch)
	--crossSwitchCount;
    dlAddTail(finishedList, m->sourceNode);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
	/* Good return - move self and source node to finished list. */
	++machinesFinished;
	dlAddTail(finishedList, finNode);
	}
    else
	{
	/* Bad return.  Increment error count, and maybe move it to
	 * error list. */
	if (++m->errCount >= maxErrCount)
	    {
	    ++machinesFinished;
	    dlAddTail(errList, finNode);
	    fprintf(stderr, "Gave up on %s\n", m->name);
	    }
	else
	    {
	    dlAddMiddle(toDoList, finNode);
	    fprintf(stderr, "Retry %d on %s\n", m->errCount, m->name);
	    }
	}
    }
if (!dlEmpty(errList))
    {
    fprintf(stderr, "errors in:");
    for (node = errList->head; node->next != NULL; node = node->next)
	{
	m = node->val;
	fprintf(stderr, " %s", m->name);
	}
    fprintf(stderr, "\n");
    }
goodMachines = dlCount(finishedList);
grandTotal = (double)goodMachines * (double)size;
printf("Copied to %d of %d machines (grand total %e bytes) in %d seconds\n", 
	goodMachines, machineCount, grandTotal, (int)(time(NULL) - startTime));
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *source, *dest, *hostList;

optionHash(&argc, argv);
crossSwitchMax = optionInt("crossMax", crossSwitchMax);
if (argc != 3 && argc != 4)
    usage();
source = argv[1];
dest = argv[2];
if (argc == 4)
    {
    hostList = argv[3];
    hostList = fullPathName(hostList);
    }
else
    hostList = "/projects/compbio/experiments/hg/switchList";
ccCp(source, dest, hostList);
return 0;
}

