#ifndef PARAHUB_H
#define PARAHUB_H

struct job
/* A job .*/
    {
    struct dlNode *node;        /* Job's node on doubly-linked list. */
    int id;			/* Uniq	job id. */
    char *cmd;                  /* Executable name plus parameters. */
    char *user;			/* User name. */
    char *dir;			/* Starting dir. */
    char *in;			/* Stdin. */
    char *out;			/* Stdout. */
    char *err;			/* Stderr. */
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
    time_t lastAlive;		/* Last time saw machine was alive in seconds past 1972 */
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
    time_t lastAlive;		/* Last time saw spoke was alive in seconds past 1972 */
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

void startHeartbeat();
/* Start heartbeat deamon. */

void endHeartbeat();
/* Kill heartbeat deamon. */

#endif /* PARAHUB_H */
