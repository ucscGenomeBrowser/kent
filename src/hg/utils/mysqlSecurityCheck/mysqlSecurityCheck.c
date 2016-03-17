/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


/* mysqlSecurityCheck - The primary reason this utility has been created is to verify that
 * access by CGI database connections to the mysql database is not allowed.
 * We will probably throw in some other useful checks at some point.
 */

#include "common.h"
#include "options.h"
#include "jksql.h"
#include "hgConfig.h"

char *database = NULL;
char *host     = NULL;
char *user     = NULL;
char *password = NULL;

struct sqlConnection *conn = NULL;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mysqlSecurityCheck - Check the mysql security configuration\n"
  "usage:\n"
  "   mysqlSecurityCheck\n"
  "\n"
  "Use the HGDB_CONF environment variable to specify which configuration to use, for example\n"
  "HGDB_CONF=/usr/local/apache/cgi-bin/hg.conf\n"
/*
  "options:\n"
*/
  );
}


static struct optionSpec options[] = {
/*
   {"chunkSize", OPTION_INT},
   {"chunkWait", OPTION_INT},
   {"squealSize", OPTION_INT},
   {"purgeStart", OPTION_INT},
   {"purgeEnd", OPTION_INT},
   {"purgeTable", OPTION_STRING},
   {"dryRun", OPTION_STRING},
*/
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
    warn("setting %s not found!",temp);
return value;
}


boolean mysqlCheckSecurityOfConfig(char *config)
/* Can we connect? Can we access the mysql database? */
{

boolean problemFound = FALSE;

if (
    sameString(config, "Xarchivecentral") ||
    sameString(config, "XcustomTracks")
    )
    {
    printf("Skipping %s for now.\n", config);
    }
else
    {
    /* get connection info */
    database = getCfgOption(config, "db"      );
    host     = getCfgOption(config, "host"    );
    user     = getCfgOption(config, "user"    );
    password = getCfgOption(config, "password");

    //uglyf("database=%s\n", database);// DEBUG REMOVE
    //uglyf("host=%s\n", host);// DEBUG REMOVE
    //uglyf("user=%s\n", user);// DEBUG REMOVE
    //uglyf("password=%s\n", password);// DEBUG REMOVE
    // it seems to tolerate connecting to a NULL database?
retry_it:
    conn = sqlMayConnectRemote(host, user, password, database);

    if (conn)
	{
    	printf("Connected to %s.\n", config);
	printf("select database() = [%s]\n", sqlQuickString(conn, NOSQLINJ "select database()"));
	char *result = sqlQuickString(conn, NOSQLINJ "show databases like 'mysql'");
	printf("show databases like 'mysql' = [%s]\n", result);
	if (result)
	    problemFound = TRUE;
	if (!problemFound)
	    {
	    char *result = sqlQuickString(conn, NOSQLINJ "SELECT table_name FROM INFORMATION_SCHEMA.TABLES WHERE table_schema = 'mysql'");
	    if (result)
		{
		problemFound = TRUE;
		printf("INFORMATION_SCHEMA is allowing access to mysql db\n");
		}
	    else
		{
		printf("INFORMATION_SCHEMA is NOT allowing access to mysql db\n");
		}
	    }
	/* Disabling this check. It actually shows information about mysql leaking out, but it does not give hackers access to passwords 
	if (!problemFound)
	    {
	    char *result = sqlQuickString(conn, NOSQLINJ "SELECT table_name FROM INFORMATION_SCHEMA.TABLES WHERE table_name = 'user'");
	    if (result)
		{
		problemFound = TRUE;
		printf("INFORMATION_SCHEMA is allowing access to user table\n");
		}
	    else
		{
		printf("INFORMATION_SCHEMA is NOT allowing access to user table\n");
		}
	    }
	*/
	if (!problemFound)
	    {
	    char *query = NOSQLINJ "desc mysql.user";
	    unsigned int errNo = 0;
	    char *errMsg = NULL;
	    struct sqlResult *rs = sqlGetResultExt(conn, query, &errNo, &errMsg);
	    if (rs)
		{
		sqlFreeResult(&rs);
		problemFound = TRUE;
		printf("desc command is leaking access to mysql.user\n");
		}
	    else
		{
		printf("desc mysql.user returned errNo=%d errMsg=[%s]\n", errNo, errMsg);
		}
	    }
	if (!problemFound)
	    {
	    char *query = NOSQLINJ "select * from mysql.user";
	    unsigned int errNo = 0;
	    char *errMsg = NULL;
	    struct sqlResult *rs = sqlGetResultExt(conn, query, &errNo, &errMsg);
	    if (rs)
		{
		sqlFreeResult(&rs);
		problemFound = TRUE;
		printf("select * from mysql.user is leaking access to mysql database\n");
		}
	    else
		{
		printf("select * from mysql.user returned errNo=%d errMsg=[%s]\n", errNo, errMsg);
		}
	    }
	if (!problemFound)
	    {
	    char *query = NOSQLINJ "use mysql";
	    unsigned int errNo = 0;
	    char *errMsg = NULL;
	    struct sqlResult *rs = sqlGetResultExt(conn, query, &errNo, &errMsg);
	    if (errNo == 0)
		{
		sqlFreeResult(&rs);
		problemFound = TRUE;
		printf("use mysql is leaking access to mysql database\n");
		}
	    else
		{
		printf("use mysql returned errNo=%d errMsg=[%s]\n", errNo, errMsg);
		}
	    }
	}
    else
    	printf("Connection to %s failed.\n", config);

    if (!conn && database)
	{
	database = NULL;
	printf("retrying connect with NULL database\n");
	goto retry_it;
	}

    sqlDisconnect(&conn);
    }

return problemFound;

}



int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if ((argc != 1) || optionExists("-help"))
    usage();

char *hgdbConf = getenv("HGDB_CONF");
if (hgdbConf)
    printf("\nHGDB_CONF = %s\n\n", hgdbConf);

// go through the list of cfgNames found to assemble a list of configs
struct slName *cfgName = NULL, *cfgNameList = NULL, *configList = NULL, *config = NULL;
cfgNameList = cfgNames();
slNameSort(&cfgNameList);
for (cfgName = cfgNameList; cfgName; cfgName = cfgName->next)
    {
    //printf("%s\n", cfgName->name);
    if (endsWith(cfgName->name, ".user"))
	{
	char *base = cloneString(cfgName->name);
	base[strlen(base)-5] = 0;
	/* get connection info */
	//char *database = getCfgOption(base, "db"      );  apparently this can be optional on customtracks?
	char *host     = getCfgOption(base, "host"    );
	char *user     = getCfgOption(base, "user"    );
	char *password = getCfgOption(base, "password");
	if (/*database &&*/ host && user && password)
	    {
    	    //printf("%s\n", base);
	    slAddHead(&configList, slNameNew(base));
	    }
	}
    }
slReverse(&configList);
printf("Configs found:\n\n");
int problemsFound = 0;
for (config = configList; config; config = config->next)
    {
    printf("Config %s\n", config->name);
    printf("%s.host=%s\n", config->name, getCfgOption(config->name, "host"));
    printf("%s.user=%s\n", config->name, getCfgOption(config->name, "user"));
    boolean problemFound = mysqlCheckSecurityOfConfig(config->name);
    if (problemFound)
	{
	++problemsFound;
	printf("!!! Problem found in config %s !!!\n", config->name);
	}
    else
	{
	printf("Config %s is protected from mysql database access.\n", config->name);
	}
    printf("\n");
    }
printf("\nProblems Found in %d out of %d Configs\n\n", problemsFound, slCount(configList));

if (problemsFound > 1)
    return 1;

return 0;  
}
