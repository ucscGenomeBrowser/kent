#ifndef PARAHUB_H
#define PARAHUB_H

struct job
/* A job .*/
    {
    struct dlNode *node;        /* Job's node on doubly-linked list. */
    int id;			/* Uniq	job id. */
    char *exe;			/* Executable name - no path or parameters. */
    char *cmd;                  /* Executable name plus parameters. */
    char *user;			/* User name. */
    char *dir;			/* Starting dir. */
    char *in;			/* Stdin. */
    char *out;			/* Stdout. */
    char *err;			/* Stderr. */
    char *results;		/* File to append results to. */
    time_t submitTime;          /* Time job submitted. */
    time_t startTime;           /* Start job run time in seconds past 1972 */
    struct machine *machine;	/* Machine it's running on if any. */
    };

struct machine
/* A machine for running jobs on. */
    {
    struct machine *next;	/* Next in master list. */
    struct dlNode *node;        /* List node of machine. */
    char *name;                 /* Name.  Not alloced here. */
    struct job *job;		/* Current job if any. */
    int errCount;               /* Number of errors. */
    time_t lastChecked;		/* Last time we checked machine in seconds past 1972 */
    boolean isDead;		/* True if machine dead. */
    char *tempDir;		/* Name of local temp dir. */
    };

struct spoke
/* A spoke - a process that initiates connections with remote hosts.
 * Parasol sends messages through spokes so as not to get bogged down
 * waiting on a remote host.  Parasol recieves all messages through it's
 * central socket though.  Typically hub server will have 10-100 spokes. */
    {
    struct spoke *next;		/* Next spoke in master list. */
    struct dlNode *node;	/* Node on doubly-linked list. */
    int id;			/* ID of spoke. */
    int socket;			/* Open socket to spoke. */
    char *socketName;		/* File name of socket. */
    int pid;			/* Process ID. */
    char *machine;		/* Machine this spoke is communicating with or NULL */
    time_t lastPinged;		/* Last time we pinged spoke. */
    int pingCount;		/* Number of times we've pinged spoke. */
    };

struct resultQueue
/* A place to write job results to. */
    {
    struct resultQueue *next;
    char *name;		/* Name of queue (and file) */
    FILE *f;		/* File handle */
    time_t lastUsed;	/* Last time this was used. */
    };

struct spoke *spokeNew(int *closeList);
/* Get a new spoke.  Close list is an optional, -1 terminated array of file
 * handles to close. This will fork and create a process for spoke and a 
 * socket for communicating with it. */

void spokeFree(struct spoke **pSpoke);
/* Free spoke.  Close socket and kill process associated with it. */

void spokeSendMessage(struct spoke *spoke, struct machine *machine, char *message);
/* Send a generic message to machine through spoke. */

void spokeSendJob(struct spoke *spoke, struct machine *machine, struct job *job);
/* Send a job to machine through spoke. */

void spokePing(struct spoke *spoke);
/* Send ping message to spoke.  It should eventually respond
 * with recycleSpoke message to hub socked.  */

void startHeartbeat();
/* Start heartbeat deamon. */

void endHeartbeat();
/* Kill heartbeat deamon. */

int hubConnect();
/* Return connection to hub socket - with paraSig already written. */

extern char *hubHost;	/* Name of machine running this. */
extern char hubHAddress[32]; /* Host address of machine running this. Not IP address. 
			      * Just for use between hub daemon and spokes*/

void logIt(char *format, ...);
/* Print message to log file. */

#define MINUTE 60

#endif /* PARAHUB_H */
