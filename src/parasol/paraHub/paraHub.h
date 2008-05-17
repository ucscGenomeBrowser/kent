#ifndef PARAHUB_H
#define PARAHUB_H

#include <pthread.h>

#ifndef PARALIB_H
#include "paraLib.h"
#endif /* PARALIB_H */

#ifndef PARAMESSAGE_H
#include "paraMessage.h"
#endif /* PARAMESSAGE_H */

struct job
/* A job .*/
    {
    struct dlNode *jobNode;	/* Job's node in machine.jobs. */
    struct dlNode *node;        /* Job's node on doubly-linked list. */
    int id;			/* Uniq	job id. */
    char *exe;			/* Executable name - no path or parameters. */
    char *cmd;                  /* Executable name plus parameters. */
    struct batch *batch;	/* Batch this job is in. */
    char *dir;			/* Starting dir. */
    char *in;			/* Stdin. */
    char *out;			/* Stdout. */
    char *err;			/* Stderr. */
    time_t submitTime;          /* Time job submitted. */
    time_t startTime;           /* Start job run time in seconds past 1972 */
    time_t lastClockIn;		/* Last time we got a message from machine about job. */
    struct machine *machine;	/* Machine it's running on if any. */
    };

struct machine
/* A machine for running jobs on. */
    {
    struct machine *next;	/* Next in master list. */
    struct dlNode *node;        /* List node of machine. */
    char *name;                 /* Name.  Not alloced here. */
    struct dlList *jobs;        /* List of current jobs. */
    int errCount;               /* Number of errors. */
    int goodCount;		/* Number of good runs. */
    time_t lastChecked;		/* Last time we checked machine in seconds past 1972 */
    boolean isDead;		/* True if machine dead. */
    char *tempDir;		/* Name of local temp dir. */
    struct slInt *deadJobIds;	/* List of job Ids that machine was running when it died. */
    bits32	ip;		/* IP address in host order. */
    struct machSpec *machSpec;  /* Machine spec of resources */
    };

struct batch
/* A batch of jobs from a user. */
    {
    struct batch *next;		/* Next in master list. */
    struct dlNode *node;	/* Node in active/done list. */
    char *name;			/* Batch name.  Not allocated here. */
    struct user *user;		/* User this is associated with. */
    struct dlList *jobQueue;	/* List of jobs not running */
    int runningCount;		/* Number of jobs running */
    int queuedCount;		/* Number of jobs queued */
    int doneCount;		/* Number of jobs done */
    int crashCount;		/* Number of jobs crashed */
    int continuousCrashCount;	/* Number of jobs crashed in a row */
    int priority;   		/* Priority of batch, 1 is highest priority */
    int maxNode;    		/* maxNodes for batch, -1 is no limit */
    struct hash *sickNodes;     /* Hash of nodes that have failed */
    };

struct user
/* One of our users. */
    {
    struct user *next;		/* Next in master list. */
    struct dlNode *node;	/* Node in active/idle list. */
    char *name;			/* User name. Not allocated here. */
    struct dlList *curBatches;	/* Current active batches. */
    struct dlList *oldBatches;	/* Inactive batches. */
    int runningCount;		/* Count of jobs currently running. */
    int doneCount;		/* Count of jobs finished. */
    int priority;   		/* Priority of user, 1 is highest priority */
    int maxNode;    		/* maxNodes for user, -1 is no limit */
    struct hash *sickNodes;     /* Hash of nodes that have failed */
    };

struct spoke
/* A spoke - a process that initiates connections with remote hosts.
 * Parasol sends messages through spokes so as not to get bogged down
 * waiting on a remote host.  Parasol recieves all messages through it's
 * central socket though.  Typically hub server will have 10-100 spokes. */
    {
    struct spoke *next;		  /* Next spoke in master list. */
    struct dlNode *node;	  /* Node on doubly-linked list. */
    char *name;		  	  /* Spoke name. */
    pthread_t thread;		  /* Concurrent thread data structure for spoke. */
    struct paraMessage *message;  /* Message to send to spoke or NULL. */
    pthread_mutex_t messageMutex; /* Mutex lock on message. */
    pthread_cond_t messageReady;  /* Mutex condition on message. */
    };

struct resultQueue
/* A place to write job results to. */
    {
    struct resultQueue *next;
    char *name;		/* Name of queue (and file) */
    FILE *f;		/* File handle */
    time_t lastUsed;	/* Last time this was used. */
    };

struct spoke *spokeNew();
/* Get a new spoke.  This will create a thread for the spoke and
 * initialize the message synchronization stuff. */

void spokeFree(struct spoke **pSpoke);
/* Free spoke.  Close socket and kill process associated with it. */

void spokeSendMessage(struct spoke *spoke, struct machine *machine, char *message);
/* Send a generic message to machine through spoke. */

void spokeSendJob(struct spoke *spoke, struct machine *machine, struct job *job);
/* Send a job to machine through spoke. */

void startHeartbeat();
/* Start heartbeat deamon. */

void endHeartbeat();
/* Kill heartbeat deamon. */

struct paraMessage *hubMessageGet();  
/* Get message from central queue, waiting if necessary for one to appear. 
 * Do a paraMessageFree when you're done with this message. */

void hubMessagePut(struct paraMessage *pm);
/* Add message to central queue.  Message must be dynamically allocated. */
     
void hubMessageQueueInit();
/* Setup stuff for hub message queue.  Must be called once
 * at the beginning before spawning threads. */

void sockSuckStart(struct rudp *ru);
/* Start socket sucker deamon.  */

extern char *hubHost;	/* Name of machine running this. */
extern char hubHAddress[32]; /* Host address of machine running this. Not IP address. 
			      * Just for use between hub daemon and spokes*/
extern unsigned char hubSubnet[4];   /* Subnet to check. */

void logIt(char *format, ...);
/* Print message to log file. */

#define uglyLog logIt

#define MINUTE 60

#endif /* PARAHUB_H */
