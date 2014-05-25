/* eapLib - library shared by analysis pipeline modules */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef EAPLIB_H
#define EAPLIB_H

extern char *eapEdwCacheDir;
/* Where data warehouse files are cached in a place that the cluster can access. */

extern char *eapValDataDir;
/* Where information sufficient to validate a file lives.  This includes genomes of
 * several species indexed for alignment. */

extern char *eapTempDir;
/* This temp dir will contain a subdir for each job.  The edwFinish program will
 * remove these if the job went well.  If the job didn't go well they'll probably
 * be empty.  There's some in-between cases though. */

extern char *eapJobTable;
/* Main analysis job table in encodeDataWarehouse. */

extern char *eapParaHost;
/* Parasol host name. A machine running paraHub */

char *eapParaDirs(struct sqlConnection *conn);
/* Root directory to parasol job results queues, where parasol (eventually) stores
 * results of jobs that successfully complete or crash. */

extern char *eapSshArgs;
/* Arguments to pass to ssh or scp for good performance and security */

struct sqlConnection *eapConnect();
/* Return read-only connection to eap database (which may be same as edw database) */

struct sqlConnection *eapConnectReadWrite();
/* Return read/write connection to eap database, which may be same as edw database) */

struct edwUser *eapUserForPipeline(struct sqlConnection *conn);
/* Get user associated with automatic processes and pipeline submissions. */

void eapPathForCommand(char *command, char path[PATH_LEN]);
/* Figure out path associated with command */

void eapMd5ForCommand(char *command, char md5[33]);
/* Figure out md5 associated with command */

struct paraPstat2Job *eapParasolRunningList(char *paraHost);
/* Return list of running jobs  in paraPstat2Job format. */

struct hash *eapParasolRunningHash(char *paraHost, struct paraPstat2Job **retList);
/* Return hash of parasol IDs with jobs running. Hash has paraPstat2Job values.
 * Optionally return list as well as hash */

char *eapStepFromCommandLine(char *commandLine);
/* Given command line looking like 'edwCdJob step parameters to program' return 'step' */

struct eapStep *eapStepFromName(struct sqlConnection *conn, char *name);
/* Get eapStep record from database based on name. */

struct eapStep *eapStepFromNameOrDie(struct sqlConnection *conn, char *analysisStep);
/* Get analysis step of given name, or complain and die. */

struct eapSoftware *eapSoftwareFromName(struct sqlConnection *conn, char *name);
/* Get eapSoftware record by name */

unsigned eapCheckVersions(struct sqlConnection *conn, char *analysisStep);
/* Check that we are running tracked versions of everything. Returns version/step
 * id of current step. */

void eapSoftwareUpdateMd5ForStep(struct sqlConnection *conn, char *analysisStep);
/* Update MD5s on all software used by step. */

int eapJobAdd(struct sqlConnection *conn, char *commandLine, int cpusRequested);
/* Add job to edwAnalyisJob table and return job ID. */

unsigned eapCurrentStepVersion(struct sqlConnection *conn, char *stepName);
/* Returns current version (id in eapStepVersion) for step.  
 * Behind the scenes it checks the software used by step against the current
 * step version.  If any has changed (or if no version yet exists) it creates
 * a new version and returns it.  In case of no change it just returns id
 * of currently tracked version. */

enum eapRedoPriority {erpNoRedo=-1, erpUnknown=0, eapUpgrade=1, eapRedo=2};
/* Definitions for what fits in eapSwVersion.redoPriority field */

#endif /* EAPLIB_H */

