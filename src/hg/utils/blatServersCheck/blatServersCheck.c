/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */


/* blatServersCheck - Check that the blatServers table value match the 
 * actual running gfServers.
 */

#include "common.h"
#include "options.h"
#include "jksql.h"
#include "hgConfig.h"
#include "genoFind.h"
#include "net.h"
#include "hash.h"
#include "obscure.h"

char *database = NULL;
char *host     = NULL;
char *user     = NULL;
char *password = NULL;

struct sqlConnection *conn = NULL;

char *blatServersTableName = "blatServers";

int timeout = 10;  // default timeout in seconds

char *hostFilter = NULL;  // default to searching all hosts

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blatServersCheck - Check the blatServers table matches running gfServers\n"
  "usage:\n"
  "   blatServersCheck profile\n"
  "profile should be from hg.conf like central or rrcentral\n"
  "options:\n"
  " -timeout=10 timeout in seconds.\n"
  " -host=blatx only check records with matching host.\n"
  " -verbose=2  can show details for each server.\n"
  "\n"
  );
}


static struct optionSpec options[] = {
   {"timeout", OPTION_INT},
   {"host",  OPTION_STRING},
   {"-help", OPTION_BOOLEAN},
   {NULL, 0},
};

char *getCfgOption(char *config, char *setting)
/* get setting for specified config */
{
char temp[256];
safef(temp, sizeof(temp), "%s.%s", config, setting);
char *value = cfgOption(temp);
if (!value)
    errAbort("setting %s not found!",temp);
return value;
}

void setSocketTimeout(int sockfd, int delayInSeconds)
// put socket read timeout so it will not take forever to timeout during a read
{
struct timeval tv;
tv.tv_sec = delayInSeconds;
tv.tv_usec = 0;
setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
}

int statusServer(char *hostName, char *portName, boolean isTrans, boolean isDynamic, char *db, struct hash *versionHash)
/* Send status message to server arnd report result.
 * Returns -1 for connection error.
 * Returns -2 for error reading string.
 * Returns -3 for type mismatch.
 * Returns -4 for host mismatch.
 * Returns -5 for port mismatch.
 */
{
char buf[256];
int sd = 0;
int ret = 0;

/* Put together command. */
sd = netConnectWithTimeout(hostName, atoi(portName), timeout * 1000); // 10 seconds connect timeout
if (sd == -1)
    {
    warn("Error connecting to %s:%s",  hostName, portName);
    return -1;
    }

setSocketTimeout(sd, timeout);

if (isDynamic)
    sprintf(buf, "%s%s %s %s", gfSignature(), isTrans ? "transInfo" : "untransInfo", db, db);
else
    sprintf(buf, "%sstatus", gfSignature());
mustWriteFd(sd, buf, strlen(buf));

for (;;)
    {
    if (netGetString(sd, buf) == NULL)
        {
        warn("Error reading status information from %s:%s",  hostName, portName);
        ret = -2;  // error reading response
        break;
        }
    if (sameString(buf, "end"))
	{
        break;
	}
    else
	{
	verbose(2, "%s\n", buf);
        if (startsWith("version ", buf))
	    {
	    char *hashKey = buf+strlen("version ");
	    hashIncInt(versionHash, hashKey);   // count how many with this version
	    }
        if (startsWith("type ", buf))
	    {	    
	    if (sameString(buf, "type translated") && !isTrans)
		{
		warn("type mismatch: gfServer says type translated but db says isTrans==0");
		ret = -3;  // type mismatch
		break;
		}
	    if (sameString(buf, "type nucleotide") && isTrans)
		{
		warn("type mismatch: gfServer says type nucleotide but db says isTrans==1");
		ret = -3;  // type mismatch
		break;
		}
	    }
        if (startsWith("host ", buf))
	    {
	    if (!sameString(buf+strlen("host "), hostName))
		{
		warn("host mismatch: gfServer says %s but db says db=%s", buf, hostName);
		ret = -4;  // host mismatch
		break;
		}
	    }
        if (startsWith("port ", buf))
	    {
	    if (!sameString(buf+strlen("port "), portName))
		{
		warn("port mismatch: gfServer says %s but db says port=%s", buf, portName);
		ret = -5;  // port mismatch  // probably never happens.
		break;
		}
	    }
	}
    }
close(sd);
return(ret);
}


int getFileList(char *hostName, char *portName, char *db, struct hash *targetDbHash)
/* Get and display input file list. 
 * Returns -1 for 2bit name mismatch
 * Returns -2 for name mot found in targetDb, probably obsolete
 * Returns -3 for targetDb.seqFile does not match fileName on gfServer
 */
{
char buf[256];
int sd = 0;
int fileCount;
int i;
int ret = 0;
char twoBitName[256];

safef(twoBitName, sizeof twoBitName, "%s.2bit", db);

/* Put together command. */
sd = netConnectWithTimeout(hostName, atoi(portName), timeout * 1000); // 10 seconds connect timeout

setSocketTimeout(sd, timeout);

sprintf(buf, "%sfiles", gfSignature());
mustWriteFd(sd, buf, strlen(buf));

/* Get count of files, and then each file name. */
if (netGetString(sd, buf) != NULL)
    {
    fileCount = atoi(buf);
    for (i=0; i<fileCount; ++i) // could have multiples if .nib used?
	{
	char *fileName = netRecieveString(sd, buf);
	//printf("%s\n", fileName);
	verbose(2, "%s\n", fileName);
	// if .nib, cannot confirm the name.
	if (endsWith(fileName,".nib"))
            {
	    // DO NOTHING FOR NIBS
	    }
        else if (strstr(db, "Kg"))
	    {
	    struct hashEl *hel = hashLookup(targetDbHash, db);
	    if (hel == NULL)
		{
		warn("db %s not found in targetDb.name, probably obsolete", db);
		ret = -2;  // name mot found in targetDb, probably obsolete
		break;
		}
	    else
		{
		char *seqFile = hel->val;
		if (!endsWith(seqFile, fileName))
		    {
		    warn("targetDb.seqFile %s does not match fileName=%s on gfServer", seqFile, twoBitName);
		    ret = -3;  // targetSeq fileName mistmatch
		    break;
		    }
		}
	    }
        else
	    {
	    if (!sameString(fileName, twoBitName))
		{
		warn("2bit name mismatch fileName=%s twoBitName=%s", fileName, twoBitName);
		ret = -1;  // 2bit name mismatch
		break;
		}
	    }
	}
    }
close(sd);
return ret;
}

void getTargetDb(struct sqlConnection *conn, struct hash *targetDbHash)
/* read targetDb table into hash */
{
struct sqlResult *sr;
char **row;
char query[256];

sqlSafef(query,sizeof(query), "select name, seqFile from targetDb");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name    = row[0];
    char *seqFile = row[1];
    struct hashEl *hel = hashLookup(targetDbHash, name);
    if (hel == NULL)
	{
	hashAdd(targetDbHash, name, cloneString(seqFile));
	}
    else
	{
	warn("duplicate targetDb.name found %s", name);
	}
    }

sqlFreeResult(&sr);
} 

void blatServersCheck(char *config)
/* Check blatServers table against running gfServers */
{

struct hash *hash = hashNew(12);
struct hash *versionHash = hashNew(6);
struct hash *targetDbHash = hashNew(6);

char hashKey[256];

/* get connection info */
database = getCfgOption(config, "db"      );
host     = getCfgOption(config, "host"    );
user     = getCfgOption(config, "user"    );
password = getCfgOption(config, "password");

conn = sqlConnectRemote(host, user, password, database);

getTargetDb(conn, targetDbHash);

struct sqlResult *sr;
char **row;
char query[256];
int totalRows = 0;

verbose(1, "-------------------\n");
verbose(1, "Reading table %s\n", blatServersTableName);


totalRows = sqlTableSize(conn, blatServersTableName);
verbose(1,"totalRows=%d\n", totalRows);

verbose(1,"\n");

if (totalRows==0)
    {
    errAbort("table %s is empty!", blatServersTableName);
    }

int errCount = 0;

sqlSafef(query,sizeof(query), "select db, host, port, isTrans, dynamic from %s order by db,port", blatServersTableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *db      = row[0];
    char *host    = row[1];
    char *portStr = row[2];
    unsigned int port  = sqlUnsigned(portStr);
    unsigned int trans = sqlUnsigned(row[3]);
    unsigned int dynamic = sqlUnsigned(row[4]);
    boolean isTrans = (trans == 1);
    boolean isDynamic = (dynamic == 1);

    if (hostFilter && !sameString(hostFilter, host))
	continue;   // skip

    int res = statusServer(host, portStr, isTrans, isDynamic, db, versionHash);
    int res2 = 0;
    if (res != -1)  // if not a connection error, proceed.
	{
	res2 = getFileList(host, portStr, db, targetDbHash);
	}
    safef(hashKey, sizeof hashKey, "%s:%s", host, portStr);
    struct hashEl *hel = hashLookup(hash, hashKey);
    if (hel == NULL)
	{
	hashAdd(hash, hashKey, NULL);
	}
    else
	{
	if (!isDynamic)
	    warn("duplicate host:port %s found", hashKey);
	}

    if ((res != 0) || (res2 != 0) || ((hel != NULL) && !isDynamic))
	{
	++errCount;
    	verbose(1, "db=%s host=%s port=%d isTrans=%d res=%d res2=%d\n\n", db, host, port, trans, res, res2);
	}
    else
	{
    	verbose(2, "db=%s host=%s port=%d isTrans=%d res=%d res2=%d\n\n", db, host, port, trans, res, res2);
	}
    }


sqlFreeResult(&sr);
sqlDisconnect(&conn);

// Show a count of how many servers were of each gfServer version
struct hashEl *el, *list = hashElListHash(versionHash);
slSort(&list, hashElCmp);
verbose(1, "version count\n");
verbose(1, "-------------\n");
for (el = list; el != NULL; el = el->next)
   {
   verbose(1, "%s %d\n", el->name, ptToInt(el->val));
   }
hashElFreeList(&list);

verbose(1, "\n");

if (errCount > 0)
    errAbort("error count=%d", errCount);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if ((argc != 2) || optionExists("-help"))
    usage();

timeout = optionInt("timeout", timeout);

hostFilter = optionVal("host", hostFilter);

blatServersCheck(argv[1]);

printf("No errors found!\n");

return 0;  
}
