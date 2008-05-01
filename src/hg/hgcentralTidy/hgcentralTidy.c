
/* hgcentralTidy - Clean out old cart records from userDb and sessionDb tables 
 * in an hgcentral db by processing it in chunks so that it 
 * won't lock out other cgi processes that are running.
 * Also, check that maximum table size has not been exceeded,
 * and send warning to cluster-admin if it has.
 */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "hgConfig.h"

char *database = NULL;
char *host     = NULL;
char *user     = NULL;
char *password = NULL;

struct sqlConnection *conn = NULL;


int chunkSize = 1000;
int chunkWait = 0;
int squealSize = 14;   /* complain if table data_length is bigger than squealSize GB */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgcentralTidy - Clean out old carts in hgcentral without blocking cart use\n"
  "usage:\n"
  "   hgcentralTidy config\n"
  "options:\n"
  "   -chunkSize=N - number of rows to examine in one chunk, default %d\n"
  "   -chunkWait=N - sleep interval between chunks to allow other processing, default %d'\n"
  "   -squealSize=N - email warning to cluster-admin when this size in GB is exceeded, default %d'\n"
  , chunkSize
  , chunkWait
  , squealSize
  );
}


static struct optionSpec options[] = {
   {"chunkSize", OPTION_INT},
   {"chunkWait", OPTION_INT},
   {"squealSize", OPTION_INT},
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

boolean cleanTable(char *table)
/* clean a specific table */
{

struct sqlResult *sr;
char **row;
char query[256];
int rc = 0;
int dc = 0;
int delCount = 0;
int count = 0;
int maxId = 0;
int useCount = 0;
boolean	deleteThis = FALSE;
boolean squealed = FALSE;
long long dataLength = 0;
long long dataFree = 0;
time_t cleanStart = time(NULL);

struct dyString *dy = dyStringNew(0);

verbose(1, "-------------------\n");
verbose(1, "Cleaning table %s\n", table);
verbose(1, "%s\n", ctime(&cleanStart));
while(TRUE)
    {
    verbose(2, "maxId: %d   count=%d  delCount=%d   dc=%d\n", maxId, count, delCount, dc);

    dyStringClear(dy);
    dyStringPrintf(dy, "delete from %s where id in (", table);

    safef(query,sizeof(query),
	"select id, firstUse, lastUse, useCount from %s"
	" where id > %d order by id limit %d"
	, table
	, maxId
        , chunkSize
	);
    sr = sqlGetResult(conn, query);
    rc = 0;
    dc = 0;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	++count;
	++rc;

        maxId = sqlUnsigned(row[0]);
        useCount = sqlSigned(row[3]);

	struct tm *tmFirstUse, *tmLastUse;
	AllocVar(tmFirstUse);
	AllocVar(tmLastUse);
	if (!strptime(row[1], "%Y-%m-%d %H:%M:%S", tmFirstUse))
	    errAbort("strptime failed for %s", row[1]);
	if (!strptime(row[2], "%Y-%m-%d %H:%M:%S", tmLastUse))
	    errAbort("strptime failed for %s", row[1]);

	time_t firstUse, lastUse, now;
        now = time(NULL);
	firstUse = mktime(tmFirstUse);
	lastUse = mktime(tmLastUse);
        int daysAgoFirstUse = difftime(now, firstUse) / (60*60*24); 
        int daysAgoLastUse = difftime(now, lastUse) / (60*60*24); 

	verbose(3, "id: %d, firstUse: [%s] [%d days ago], lastUse: [%s] [%d days ago], useCount: %d\n"
	    , maxId
	    , row[1], daysAgoFirstUse
	    , row[2], daysAgoLastUse
	    , useCount 
	    );

	deleteThis = FALSE;

	if (sameString(table, "sessionDb"))
	    {
	    if (daysAgoLastUse >= 14)
		{
		deleteThis = TRUE;
		}
	    else if ((daysAgoFirstUse >= 2) && useCount <= 1)  /* reasonable new addition */
		{
		deleteThis = TRUE;
		}
	    }

	if (sameString(table, "userDb"))
	    {
	    if ((daysAgoFirstUse >= 7) && useCount < 7)
		{
		deleteThis = TRUE;
		}
	    else if ((daysAgoFirstUse >= 2) && useCount <= 1)
		{
		deleteThis = TRUE;
		}
	    else if (daysAgoLastUse >= 365)  /* reasonable new addition */
		{
		deleteThis = TRUE;
		}
	    }

	if (deleteThis)
	    {
    	    ++dc;
	    verbose(3, "TO DELETE id: %d, "
    		"firstUse: [%s] [%d days ago], lastUse: [%s] [%d days ago], useCount: %d\n"
		, maxId
    		, row[1], daysAgoFirstUse
    		, row[2], daysAgoLastUse
    		, useCount 
    		);
	    dyStringPrintf(dy, "%d,", maxId);
	    }

	}
    sqlFreeResult(&sr);
    
    if (rc < 1)
	break;   /* we must be all done */

    delCount+=dc;

    if (dc > 0)
	{
	--dy->stringSize;  /* delete trailing comma */
	dyStringPrintf(dy, ")");
	verbose(3, "DELETE QUERY: [%s]\n", dy->string);
	
	sqlUpdate(conn,dy->string);
	}


    sleep(chunkWait);

    //if (count >= 5000)    
    	//break; // debug

    }

verbose(1, "%s: #rows count=%d  delCount=%d\n\n", table, count, delCount);


time_t cleanEnd = time(NULL);
int minutes = difftime(cleanEnd, cleanStart) / 60; 

verbose(1, "%s\n", ctime(&cleanEnd));
verbose(1, "%d minutes\n\n", minutes);


/* check max table size has been exceeded, send email warning if so */
safef(query, sizeof(query), "show table status like '%s'", table );
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (!row)
    errAbort("error fetching table status");
int dlField = sqlFieldColumn(sr, "Data_length");
if (dlField == -1)
    errAbort("error finding field 'Data_length' in show table status resultset");
dataLength = sqlLongLong(row[dlField]);
int dfField = sqlFieldColumn(sr, "Data_free");
if (dfField == -1)
    errAbort("error finding field 'Data_free' in show table status resultset");
dataFree = sqlLongLong(row[dfField]);
verbose(1, "%s: Data_length=%lld Data_free=%lld\n\n", table, dataLength, dataFree);
if ((dataLength / (1024 * 1024 * 1024)) >= squealSize)
    {
    char msg[256];
    char cmdLine[256];
    char *emailList = "cluster-admin galt kuhn";
    safef(msg, sizeof(msg), "BIG HGCENTRAL TABLE %s data_length: %lld data_free: %lld\n"
	, table, dataLength, dataFree);
    printf("%s", msg);
    safef(cmdLine, sizeof(cmdLine), 
	"echo '%s'|mail -s 'WARNING hgcentral cleanup detected data_length max size %d GB exceeded' %s"
	, msg
	, squealSize
	, emailList
	);
    system(cmdLine);
    squealed = TRUE;

    }
sqlFreeResult(&sr);

return squealed;

}

boolean hgcentralTidy(char *config)
/* hgcentralTidy - Clean out old carts. */
{

boolean squealed = FALSE;

/* get connection info */
database = getCfgOption(config, "db"      );
host     = getCfgOption(config, "host"    );
user     = getCfgOption(config, "user"    );
password = getCfgOption(config, "password");

conn = sqlConnectRemote(host, user, password, database);

verbose(1, "Cleaning database %s.%s\n", host, database);
verbose(1, "chunkWait=%d chunkSize=%d\n", chunkWait, chunkSize);
if (cleanTable("sessionDb"))
  squealed = TRUE;
if (cleanTable("userDb"))
  squealed = TRUE;

sqlDisconnect(&conn);

return squealed;

}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
chunkSize = optionInt("chunkSize", chunkSize);
chunkWait = optionInt("chunkWait", chunkWait);
squealSize = optionInt("squealSize", squealSize);
if (argc != 2)
    usage();
return hgcentralTidy(argv[1]);
}
