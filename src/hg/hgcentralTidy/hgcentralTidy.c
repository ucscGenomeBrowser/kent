/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */


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
int squealSize = 20;  /* complain if table data_length is bigger than squealSize GB */
 // was 14 until 2018-06-02  

int purgeStart = -1;  /* manual specify purge range in days ago */
int purgeEnd = -1;
char *purgeTable = NULL;  /* optionally specify one table to purge */

char *sessionDbTableName = "sessionDb";
char *userDbTableName = "userDb";

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
  "   -purgeStart=N - purge range starts N days ago'\n"
  "   -purgeEnd=N - purge range end N days ago'\n"
  "   -purgeTable=tableName - optional purge table must be userDb or sessionDb. If not specified, both tables are purged.'\n"
  "   -dryRun - option that causes it to skip the call to cleanTableSection.'\n"
  "   -skipDel - option that causes it to skip the delete returning counts from skipped deletes.'\n"
  , chunkSize
  , chunkWait
  , squealSize
  );
}


static struct optionSpec options[] = {
   {"chunkSize", OPTION_INT},
   {"chunkWait", OPTION_INT},
   {"squealSize", OPTION_INT},
   {"purgeStart", OPTION_INT},
   {"purgeEnd", OPTION_INT},
   {"purgeTable", OPTION_STRING},
   {"dryRun", OPTION_STRING},
   {"skipDel", OPTION_BOOLEAN},
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


boolean checkMaxTableSizeExceeded(char *table)
/* check if max table size has been exceeded, send email warning if so */
{
boolean squealed = FALSE;
long long dataLength = 0;
long long dataFree = 0;
struct sqlResult *sr;
char **row;
char query[256];
sqlSafef(query, sizeof(query), "show table status like '%s'", table );
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
if (((dataLength-dataFree) / (1024 * 1024 * 1024)) >= squealSize)
    {
    char msg[256];
    char cmdLine[256];
    char *emailList = "cluster-admin@soe.ucsc.edu galt@soe.ucsc.edu clayfischer@ucsc.edu";
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


int toDaysAgo(char *useString, unsigned int id)
/* Convert mysql datetime into days ago */
{
struct tm tmUse;
zeroBytes(&tmUse, sizeof(struct tm));
if (!strptime(useString, "%Y-%m-%d %H:%M:%S", &tmUse))
    errAbort("strptime failed for firstUse %s (id=%u)", useString, id);

time_t use, now;
now = time(NULL);
use = mktime(&tmUse);
return difftime(now, use) / (60*60*24); 
}

void cleanTableSection(char *table, unsigned int startId, unsigned int endId)
/* clean a specific table section */
{
struct sqlResult *sr;
char **row;
char query[256];
int rc = 0;
int dc = 0;
int delCount = 0;
int count = 0;
unsigned int maxId = startId - 1;
if (startId == 0) maxId = 0;
int useCount = 0;
boolean	deleteThis = FALSE;
int delRobotCount = 0;
int oldRecCount = 0;
struct slUnsigned *delList = NULL;
time_t cleanSectionStart = time(NULL);

struct dyString *dy = dyStringNew(0);

while(TRUE)
    {
    verbose(2, "maxId: %u   count=%d  delCount=%d   dc=%d\n", maxId, count, delCount, dc);

    sqlSafef(query,sizeof(query),
	"select id, firstUse, lastUse, useCount from %s"
	" where id > %u "
        " AND lastUse < NOW() - INTERVAL 1 HOUR"
        " order by id limit %d;"
	, table
	, maxId
        , chunkSize
	);
    sr = sqlGetResult(conn, query);
    rc = 0;
    dc = 0;
    dyStringClear(dy);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	++count;
	++rc;

        maxId = sqlUnsigned(row[0]);
        useCount = sqlSigned(row[3]);

        int daysAgoFirstUse = toDaysAgo(row[1], maxId); 
        int daysAgoLastUse  = toDaysAgo(row[2], maxId); 

	verbose(3, "id: %u, firstUse: [%s] [%d days ago], lastUse: [%s] [%d days ago], useCount: %d\n"
	    , maxId
	    , row[1], daysAgoFirstUse
	    , row[2], daysAgoLastUse
	    , useCount 
	    );

	deleteThis = FALSE;

	if (sameString(table, sessionDbTableName))
	    {
	    if (daysAgoLastUse >= 14)
		{
		deleteThis = TRUE;
		++oldRecCount;
		}
            else if ((daysAgoFirstUse >= 2) && useCount <= 1)  /* reasonable new addition */
                {
                deleteThis = TRUE;
                ++delRobotCount;
                }
	    }

	if (sameString(table, userDbTableName))
	    {
            if ((daysAgoFirstUse >= 7) && useCount < 7)
                {
                deleteThis = TRUE;
                ++delRobotCount;
                }
            /* some botnets have hgsid but do not store cookies -> useCount is always 1 */
            /* Since we excluded rows that are from the last 1 hour, this will not delete anything */
            /* from human users who just came to the site before this program ran */
            else if (useCount <= 1)
                {
                deleteThis = TRUE;
                ++delRobotCount;
                }
            else if (daysAgoLastUse >= 365)  /* reasonable new addition */
		{
		deleteThis = TRUE;
		++oldRecCount;
		}
	    }

	if (deleteThis)
	    {
    	    ++dc;
	    verbose(3, "TO DELETE id: %u, "
    		"firstUse: [%s] [%d days ago], lastUse: [%s] [%d days ago], useCount: %d\n"
		, maxId
    		, row[1], daysAgoFirstUse
    		, row[2], daysAgoLastUse
    		, useCount 
    		);
	    slAddHead(&delList, slUnsignedNew(maxId));
	    }

	}
    sqlFreeResult(&sr);
   
    if (rc < 1)
	    break;

    if (dc > 0)
	{
	struct slUnsigned *i;
	for (i=delList;i;i=i->next)
	    {
            if (!optionExists("skipDel"))
		{
		dyStringClear(dy); 
	        sqlDyStringPrintf(dy, "delete from %s where id=%u", table, i->val);
		sqlUpdate(conn,dy->string);
		}
	    else
		{
		dyStringClear(dy); 
	        sqlDyStringPrintf(dy, "delete from %s where id=%u", table, i->val);
		verbose(4,"GALT DEBUG del dystring = [%s]\n", dy->string);
		}

	    }
	slFreeList(&delList);
	}
 
    delCount+=dc;
  
    if (maxId >= endId)
	{
	break;  // we have done enough
	}

    	
    verbose(3, "sleeping %d seconds\n", chunkWait);fflush(stderr);
    sleep(chunkWait);
    verbose(3, "awake\n");fflush(stderr);

    }

    verbose(1, "old recs %s deleted %d, robot recs %s deleted %d\n", optionExists("skipDel")?"would have been":"", oldRecCount,
         optionExists("skipDel")?"would have been":"", delRobotCount);fflush(stderr);

    time_t cleanEnd = time(NULL);
    int minutes = difftime(cleanEnd, cleanSectionStart) / 60; 
    verbose(1, "%s\n", ctime(&cleanEnd));
    verbose(1, "%d minutes\n\n", minutes);
}

int binaryIdSearch(unsigned int *ids, int numIds, char *table, int daysAgo, boolean endSearch)
/* Find the array index in ids which holds the id that contains
 * the oldest record satisfying the daysAgo criterion. 
 * If not found, return -1 */
{
char query[256];
int a = 0;
int b = numIds - 1;
int m = 0;
while (TRUE)
    {
    if (a > b)
	{
	if (endSearch)
	    return b;
	else
	    return a;
	}
    m = (b + a) / 2;
    //verbose(1,"bin a=%d, b=%d, m=%d\n", a, b, m);
    while (TRUE)
	{
	sqlSafef(query, sizeof(query), "select firstUse from %s where id=%u", table, ids[m]);
	char *firstUse = sqlQuickString(conn,query);
	if (firstUse)
	    {
	    int daysAgoFirstUse = toDaysAgo(firstUse, ids[m]); 
            //verbose(1, "DEBUG: %d %d %s %d\n", m, ids[m], firstUse, daysAgoFirstUse);  // DEBUG REMOVE
	    if (daysAgoFirstUse > daysAgo)
		{
		a = m + 1;
		}
	    else if (daysAgoFirstUse == daysAgo)
		{
                if (endSearch)
		    a = m + 1;
		else
		    b = m - 1;
		}
	    else
		{
		b = m - 1;
		}
	    break;
	    }
	else // rare event: record not found, was it deleted?
	    {
	    errAbort("hgcentralTidy: unexpected error in binaryIdSearch() id %u not found in table %s", ids[m], table);
	    }
	}
    }
}


boolean cleanTable(char *table)
/* clean a specific table */
{

struct sqlResult *sr;
char **row;
char query[256];
unsigned int *ids;
long totalRows = 0;
boolean squealed = FALSE;
time_t cleanStart = time(NULL);

verbose(1, "-------------------\n");
verbose(1, "Cleaning table %s\n", table);
verbose(1, "%s\n", ctime(&cleanStart));


totalRows = sqlTableSize(conn, table);
verbose(1,"totalRows=%ld\n", totalRows);

if (totalRows==0)
    {
    verbose(1,"table %s is empty!", table);
    return FALSE;
    }

AllocArray(ids, totalRows);

// This is a super-fast query because it only needs to read the index which is cached in memory.
sqlSafef(query,sizeof(query), "select id from %s" , table);
sr = sqlGetResult(conn, query);
int i = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    ids[i++] = sqlUnsigned(row[0]);
    if (i >= totalRows)
	break;
    }
sqlFreeResult(&sr);
totalRows = i;  // in case they differed.

int purgeRangeStart = -1;
int purgeRangeEnd = -1;
if (optionExists("purgeStart"))   // manual purge range specified
    {
    purgeStart = optionInt("purgeStart", -1);
    purgeEnd = optionInt("purgeEnd", -1);
    if (purgeStart < 1 || purgeStart > 8000) // up to 20 years ago
	errAbort("Invalid purgeStart");
    if (purgeEnd < 0)
	purgeEnd = 0;
    if (purgeStart < purgeEnd)
	errAbort("purgeStart should be greater than purgeEnd (in days ago)");
    purgeRangeStart = binaryIdSearch(ids, totalRows, table, purgeStart, FALSE);
    purgeRangeEnd   = binaryIdSearch(ids, totalRows, table, purgeEnd, TRUE);
    verbose(1, "manual purge range: purgeStart %d purgeEnd %d rangeStart %d rangeEnd %d rangeSize=%d ids[rs]=%u\n", 
                                    purgeStart,   purgeEnd, purgeRangeStart, purgeRangeEnd, purgeRangeEnd-purgeRangeStart, ids[purgeRangeStart]);
    if (!optionExists("dryRun"))
	cleanTableSection(table, ids[purgeRangeStart], ids[purgeRangeEnd]);
    }
else  // figure out purge-ranges automatically
    {

    int firstUseAge = 0;
    if (sameString(table, sessionDbTableName))
	firstUseAge = 14;
    if (sameString(table, userDbTableName))
	firstUseAge = 365;

    sqlSafef(query,sizeof(query), "select dayofweek(now())");
    int day = sqlQuickNum(conn, query);

    // These old records take a long time to go through, 5k sessionDb to 55k userDb old recs to look at,
    //  and typically produce only a few hundred deletions.
    //  they are growing slowly and expire rarely, so we don't need to scan them
    //  frequently and aggressively.  So ONLY scan them once per week by doing 1/7 per day.
    // Also don't need to worry much about the 
    //  borders of the split-over-7-days divisions shifting much because the set is so nearly static.  YAWN.

    int firstUseIndex = binaryIdSearch(ids, totalRows, table, firstUseAge, FALSE);
    int oldRangeSize = (firstUseIndex - 0) / 7;
    int oldRangeStart = oldRangeSize * (day-1);
    int oldRangeEnd = oldRangeStart + oldRangeSize;

    verbose(1, "old cleaner: firstUseAge=%d firstUseIndex = %d day %d: rangeStart %d rangeEnd %d rangeSize=%d ids[oldRangeStart]=%u\n", 
        firstUseAge, firstUseIndex, day, oldRangeStart, oldRangeEnd, oldRangeEnd-oldRangeStart, ids[oldRangeStart]);

    // newly old can be expected to have some delete action
    //  these records have newly crossed the threshold into being old enough to have possibly expired.
    int newOldRangeStart = firstUseIndex;
    int newOldRangeEnd = binaryIdSearch(ids, totalRows, table, firstUseAge - 1, TRUE);
    verbose(1, "newOld cleaner: firstUseAge=%d rangeStart %d rangeEnd %d rangeSize=%d ids[newOldRangeStart]=%u\n", 
	firstUseAge, newOldRangeStart, newOldRangeEnd, newOldRangeEnd-newOldRangeStart, ids[newOldRangeStart]);
   

    // this is the main delete action of cleaning out new robots (20k to 50k or more)
    int robo1RangeStart = binaryIdSearch(ids, totalRows, table, 2, FALSE);
    int robo1RangeEnd   = binaryIdSearch(ids, totalRows, table, 0, TRUE);
    verbose(1, "robot cleaner1: twoDayIndex = %d zeroDayIndex %d rangeSize=%d ids[rs]=%u\n", 
      robo1RangeStart, robo1RangeEnd, robo1RangeEnd-robo1RangeStart, ids[robo1RangeStart]);

    int robo2RangeStart = -1;
    int robo2RangeEnd = -1;
    if (sameString(table, userDbTableName))
	{  // secondary robot cleaning only for userDb., produces a somewhat lesser, perhaps 3 to 5k deletions
	robo2RangeStart = binaryIdSearch(ids, totalRows, table, 7, FALSE);
	robo2RangeEnd   = binaryIdSearch(ids, totalRows, table, 6, TRUE);
	verbose(1, "robot cleaner2: sevenDayIndex = %d sixDayIndex %d rangeSize=%d ids[rs]=%u\n", 
	  robo2RangeStart, robo2RangeEnd, robo2RangeEnd-robo2RangeStart, ids[robo2RangeStart]);
	}

    /* cannot clean until we have all the ranges determined since deleting messes up binSearch */
    if (!optionExists("dryRun"))
	{
	verbose(1, "old cleaner:\n");
	cleanTableSection(table, ids[oldRangeStart], ids[oldRangeEnd]);
	}

    if (!optionExists("dryRun"))
	{
	verbose(1, "newOld cleaner:\n");
	cleanTableSection(table, ids[newOldRangeStart], ids[newOldRangeEnd]);
	}

    if (!optionExists("dryRun"))
	{
	verbose(1, "robot cleaner1:\n");
	cleanTableSection(table, ids[robo1RangeStart], ids[robo1RangeEnd]);
	}

    if (sameString(table, userDbTableName))
	{
	if (!optionExists("dryRun"))
	    {
	    verbose(1, "robot cleaner2:\n");
	    cleanTableSection(table, ids[robo2RangeStart], ids[robo2RangeEnd]);
	    }
	}

    }

/*
int found = binaryIdSearch(ids, totalRows, table, 1, FALSE);
if ((found >= 0) && (found < totalRows))
    verbose(1, "1 days ago found = %d, id == ids[found] = %u \n", found, ids[found]);

found = binaryIdSearch(ids, totalRows, table, 2, FALSE);
if ((found >= 0) && (found < totalRows))
    verbose(1, "2 days ago found = %d, id == ids[found] = %u \n", found, ids[found]);

found = binaryIdSearch(ids, totalRows, table, 30, FALSE);
if ((found >= 0) && (found < totalRows))
    verbose(1, "30 days ago found = %d, id == ids[found] = %u \n", found, ids[found]);

*/


	    /*
	    if (daysAgoFirstUse < 14)
		{
		hitEnd = TRUE;
                break;
		}
	    */

            /*
	    if (daysAgoFirstUse < 365)
		{
		hitEnd = TRUE;
                break;
		}
            */

// may need to pass back this data from the cleanTableSection call TODO
//verbose(1, "%s: #rows count=%d  delCount=%d\n\n", table, count, delCount);

time_t cleanEnd = time(NULL);
int minutes = difftime(cleanEnd, cleanStart) / 60; 
verbose(1, "%s\n", ctime(&cleanEnd));
verbose(1, "%d minutes total\n\n", minutes);

squealed = checkMaxTableSizeExceeded(table);

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

if (!purgeTable || sameString(purgeTable,sessionDbTableName))
    {
    if (cleanTable(sessionDbTableName))
      squealed = TRUE;
    }

if (!purgeTable || sameString(purgeTable,userDbTableName))
    {
    if (cleanTable(userDbTableName))
      squealed = TRUE;
    }

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
if (optionExists("purgeTable"))
    {
    purgeTable = optionVal("purgeTable", NULL);
    if (!sameString(purgeTable,userDbTableName) && !sameString(purgeTable,sessionDbTableName))
	errAbort("Invalid value for purgeTable option, must be userDb or sessionDb or leave option off for both.");
    }
if (argc != 2)
    usage();
return hgcentralTidy(argv[1]);
}
