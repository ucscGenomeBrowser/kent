/* trashLoad - generate trash file activity load test. */

/* Copyright (C) 2013 The Regents of the University of California
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "sqlNum.h"
#include "options.h"
#include "trashDir.h"
#include "jksql.h"
#include "hdb.h"
#include "obscure.h"

static char *testDir = "loadTest";	/* directory to write files into */
static char *mysql = "";		/* database to load tables */
static boolean mysqlTesting = FALSE;	/* TRUE when mysql database given */
static char *bedFile = "";	  /* path to a bed file for mysql testing */
static boolean filesToo = FALSE;  /* also write files during mysql testing */
static boolean onServer = FALSE;	/* for mysql speedup */
static boolean MyISAM = FALSE;	/* type of ENGINE for MySQL table creation */
static char devShmDir[PATH_LEN];	/* temporary directory for bed files */
static char **bedFileNames;	/* array of file names in devShmDir/ */
static long long tableRowsLoaded = 0;	/* total number of table rows loaded */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trashLoad - generate trash file activity load test\n"
  "usage:\n"
  "   trashLoad [options] <numFiles> <averageSize>\n"
  "   constructs a directory ./%s to write files into\n"
  "options:\n"
  "   numFiles - generate this number of files, positive integer\n"
  "   averageSize - files of this average size, positive integer\n"
  "   -testDir=<dirName> - specify a different directory than ./'%s'\n"
  "           note, this will always be relative: ./<dirName>\n"
  "           it can not be an explicit path.\n"
  "   -mysql=<databaseName> - specify a database to test mysql table loading\n"
  "                         - turns off file write testing, also needs:\n"
  "   -bedFile - a bed file to use for mysql load testing, the more lines\n"
  "              the better.\n"
  "   -filesToo - also write files during mysql testing\n"
  "   -onServer This will speed things up when running with a directory that\n"
  "             the mysql server can access.  It will be /dev/shm/bedFiles/\n"
  "             set mysqld my.cnf: secure-file-priv = /dev/shm/bedFilesn\n"
  "   -MyISAM   Use MySQL ENGINE MyISAM instead of the default InnoDB",
  testDir, testDir
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"testDir", OPTION_STRING},
   {"mysql", OPTION_STRING},
   {"bedFile", OPTION_STRING},
   {"filesToo", OPTION_BOOLEAN},
   {"onServer", OPTION_BOOLEAN},
   {"MyISAM", OPTION_BOOLEAN},
   {NULL, 0},
};

static void seedRand()
/* seed the rand function with bytes from /dev/random */
{
unsigned seed = 0;
FILE *fd = mustOpen("/dev/random", "r");
fread((unsigned char *)&seed, sizeof(unsigned), 1, fd);
carefulClose(&fd);
verbose(2, "#seed: 0x%0x = %d\n", seed, seed);
srand(seed);
}

static void createTable(struct sqlConnection *conn, char *tableName)
/* use SQL definition to construct a bed 12 track and create the track */
{
int maxChromName = 32;

char *tableFormat =
"CREATE TABLE %s (\n"
"    bin int unsigned not null, # Bin for range index\n"
"    chrom varchar(255) not null,       # Reference sequence chromosome or scaffold\n"
"    chromStart int unsigned not null,  # Start position in chromosome\n"
"    chromEnd int unsigned not null,    # End position in chromosome\n"
"    name varchar(255) not null,        # Name of item - up to 16 chars\n"
"    score int not null,                # 0-1000.  Higher numbers are darker.\n"
"    strand char(1) not null,           # + or - for strand\n"
"    thickStart int unsigned not null,  # Start of thick part\n"
"    thickEnd int unsigned not null,    # End position of thick part\n"
"    reserved int unsigned  not null,   # RGB 8 bits each as in bed\n"
"    blockCount int unsigned not null,\n"
"    blockSizes longblob not null,\n"
"    chromStarts longblob not null,\n"
"              #Indices\n"
"    INDEX(chrom(%d),bin)\n"
")%s";

struct dyString *createSql = dyStringNew(0);
if (MyISAM)
  sqlDyStringPrintf(createSql, tableFormat, tableName, maxChromName,
     " ENGINE=MyISAM DEFAULT CHARSET=latin1");
else
  sqlDyStringPrintf(createSql, tableFormat, tableName, maxChromName, "");
sqlUpdate(conn, createSql->string);
dyStringFree(&createSql);
} /* static void createTable(struct sqlConnection *conn, char *tableName) */

static void loadFileToTable(struct sqlConnection *conn, char *tableName,
   char *tabFile)
{
int loadOptions = (optionExists("onServer") ? SQL_TAB_FILE_ON_SERVER : 0);
verbose(3, "# loading '%s' into '%s.%s'\n", tabFile, mysql, tableName);
sqlLoadTabFile(conn, tabFile, tableName, loadOptions|SQL_TAB_FILE_WARN_ON_WARN);
}

/* generate normal distrubion, from information found at:
 *
  http://stackoverflow.com/questions/2325472/generate-random-numbers-following-a-normal-distribution-in-c-c

 * this formula seemed to generate a bi-normal distribution with peaks
 * at -0.5 and 0.5, so, flipping all the negative values turned it into
 * a single normal distribution with a mean near 0.5
 * the tail toward zero is a bit truncated since it doesn't go below zero
 */
#define RANDU ((double) rand()/RAND_MAX)
#define RANDN2(mu, sigma) (mu + (rand()%2 ? -1.0 : 1.0)*sigma*pow(-log(0.99999*RANDU), 0.5))
#define RANDN RANDN2(0, 1.0)

static size_t normalDistribution(long long mean, long long min, long long max)
/* return a random number from a normal distribution at the specified mean
 * within the min,max limits.  The limits distort it a bit, true.
 */
{
static int depth = 0;
double randn = RANDN2(0, 1.0);
if (randn < 0.0) { randn = -randn; }
double meanValue = 2.0 * randn * (double)mean;
size_t returnValue = (size_t)(round(meanValue + 0.5));
++depth;
if (returnValue < min)
    returnValue = min;
if (returnValue > max)
    {
    if (depth > 40)  /* do not recurse indefinately */
        {
        returnValue = max;
        depth = 1;
        }
    else
        return (normalDistribution(mean, min, max));  /* recursive ! */
    }
--depth;
return (returnValue);
}

static void readLines(char *fileName, int *retCount, char ***retLines)
/* as found in gensub2.c, read all lines of file into an array */
{
struct slName *el, *list = readAllLines(fileName);
int i=0, count = slCount(list);
char **lines;

AllocArray(lines, count);
for (el = list; el != NULL; el = el->next)
    lines[i++] = trimSpaces(el->name);
*retCount = count;
*retLines = lines;
}

static void cleanUp()
/* removing temporary bed files */
{
struct fileInfo *file, *fileList = listDirX(devShmDir, "*", FALSE);
int filesRemoved = 0;
for (file = fileList; file != NULL; file = file->next)
    {
    char pathName[PATH_LEN];
    safef(pathName, sizeof(pathName), "%s/%s", devShmDir, file->name);
    unlink(pathName);
    ++filesRemoved;
    }
if (! onServer)
    rmdir(devShmDir);
verbose(3, "# removed %d files from '%s'\n", filesRemoved, devShmDir);
}

static void writeBedFile(char *fileName, int lines, char **bedArray, int sizeOfArray)
/* write number of 'lines' to devShmDir/fileName from bedArray which
 *   has a size of sizeOfArray
 */
{
char pathName[PATH_LEN];
safef(pathName, sizeof(pathName), "%s/%s", devShmDir, fileName);
verbose(3, "# writing %d lines to '%s'\n", lines, pathName);
tableRowsLoaded += lines;
FILE *fh = mustOpen(pathName, "w");
int i = 0;
for ( i = 0; i < lines; ++i)
    {
    size_t index = normalDistribution(sizeOfArray/2, 0, sizeOfArray-1);
    fprintf(fh, "%s\n", bedArray[index]);
    }
carefulClose(&fh);
}

static void prepareBedPartitions(char *bedFile, long long numFiles, long long averageSize)
/* Read all lines of file into an array.
 * partition into various sizes of bed files into a /dev/shm/bedFiles.pid/
 * or when onServer, simply /dev/shm/bedFiles/
 * directory.
 */
{
int lineCount;
char **bedArray;
readLines(bedFile, &lineCount, &bedArray);
verbose(1, "# bedFile '%s' has %d lines\n", bedFile, lineCount);

pid_t pid = getpid();
if (onServer)
    safef(devShmDir, sizeof(devShmDir), "/dev/shm/bedFiles");
else
    safef(devShmDir, sizeof(devShmDir), "/dev/shm/bedFiles.%d", (int) pid);
makeDirs(devShmDir);
bedFileNames = (char **)needMem(sizeof(char *) * numFiles);
char maxFileName[PATH_LEN];
safef(maxFileName, sizeof(maxFileName), "%d", lineCount);
int nameLength = strlen(maxFileName);
char nameFormat[PATH_LEN];
safef(nameFormat, sizeof(nameFormat), "%%0%dd", nameLength);
verbose(1, "# constructing %lld bed files, name length: %d, name format: '%s'\n", numFiles, nameLength, nameFormat);
int i;
for ( i = 0; i < numFiles; ++i)
    {
    char fileName[PATH_LEN];
    size_t linesToWrite = normalDistribution(averageSize, 1, lineCount);
    safef(fileName, sizeof(fileName), nameFormat, (int) linesToWrite);
    writeBedFile(fileName, (int)linesToWrite, bedArray, lineCount);
    char pathName[PATH_LEN];
    safef(pathName, sizeof(pathName), "%s/%s", devShmDir, fileName);
    bedFileNames[i] = cloneString(pathName);
    }
}

static void trashLoad(long long numFiles, long long averageSize)
/* trashLoad - generate trash file activity load test. */
{
int i;
long long bytesWritten = 0;
long long maxSize = averageSize * 5;
verbose(1, "# trash load test begin, numFiles: %lld, averageSize: %lld, maximum size: %lld\n", numFiles, averageSize, maxSize);
struct tempName tn;

/* testing normalDistribution, print out a million numbers, send into
 * textHistorgram to view
 */
// int i;
// for ( i = 0; i < 1000000; ++i)
//    printf("%lld\n", (long long)normalDistribution(averageSize, 1, maxSize));

struct sqlConnection *conn = NULL;
if (mysqlTesting)
    conn = hAllocConn(mysql);

/* generate a single large buffer to write from, and initialize with values */
char *buf = needMem(maxSize+1);
int j;
for ( j = 0; j < maxSize+1; ++j )
    buf[j] =  (char)(0xff & j);

long beginLoadTest = clock1000();
for ( i = 0; i < numFiles; ++i)
  {
  char nameBuffer[1024];
  safef(nameBuffer, sizeof(nameBuffer), "lt_%d", i);
  trashDirFile(&tn, testDir, nameBuffer, ".txt");
  char *fileName = tn.forCgi;

  if (conn)
      {
      char prefix[16];
      safef(prefix, sizeof(prefix), "t%d", i);
      char *dbTableName = sqlTempTableName(conn, prefix);
      createTable(conn, dbTableName);
      loadFileToTable(conn, dbTableName, bedFileNames[i]);
      }

  if (!conn || filesToo)
      {
      FILE *fh = mustOpen(fileName, "w");
      size_t writeSize = normalDistribution(averageSize, 1, maxSize);
      bytesWritten += writeSize;
      size_t itemsWritten = fwrite(buf, writeSize, 1, fh);
    /*
      fread()  and  fwrite()  return the number of items successfully read or
      written (i.e., not the number of characters).  If an error  occurs,  or
      the  end-of-file is reached, the return value is a short item count (or
      zero).
    */
      if ( 1 != itemsWritten )
         {
         errAbort("# ERROR write error,  bytes requested: %ld != bytes written: %ld\n", (long)writeSize, (long)itemsWritten);
         }
      fclose(fh);
      }
  }

long et = clock1000() - beginLoadTest;
if (! mysqlTesting || filesToo)
    {
    double bytesPerSecond = (double)bytesWritten/(double)((double)et/1000.0);
    verbose(1, "# %lld total bytes written in %lld files\n", bytesWritten, numFiles);
    verbose(1, "# trash load test total run time: %ld millis, %.0f bytes per second\n", et, bytesPerSecond);
    verbose(1, "# %lld\t%lld\t%ld\t%.0f\n", numFiles, bytesWritten, et, bytesPerSecond);
    verbose(1, "# files\tbytes\tmillis\tbytes/sec\n");
    }

if (mysqlTesting)
    {
    double tablesPerSecond = (double)numFiles/(double)((double)et/1000.0);
    double rowsPerSecond = (double)tableRowsLoaded/(double)((double)et/1000.0);
    verbose(1, "# total table rows loaded into mysql: %lld in %lld tables, %0f rows per second\n", tableRowsLoaded, numFiles, rowsPerSecond);
    verbose(1, "# mysql load test total run time: %ld millis, %0f tables per second\n", et, tablesPerSecond);
    }

hFreeConn(&conn);	/* will close only if it was allocated */

}	/* static void trashLoad(long long numFiles, long long averageSize) */

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
filesToo = optionExists("filesToo");
onServer = optionExists("onServer");
MyISAM = optionExists("MyISAM");
testDir = optionVal("testDir", testDir);
mysql = optionVal("mysql", mysql);
long long numFiles = sqlLongLong(argv[1]);
long long averageSize = sqlLongLong(argv[2]);
if (numFiles < 1)
  errAbort("ERROR: numFiles must be a positive integer, given: %lld\n", numFiles);
if (averageSize < 1)
  errAbort("ERROR: averageSize must be a positive integer, given: %lld\n", averageSize);
seedRand();

if (strlen(mysql) > 0)
    {
    bedFile = optionVal("bedFile", bedFile);
    if (strlen(bedFile) < 1)
        errAbort("ERROR: must have a -bedFile=file.bed to do mysql testing");
    verbose(1, "# testing mysql with bedFile: '%s'\n", bedFile);
    prepareBedPartitions(bedFile, numFiles, averageSize);
    mysqlTesting = TRUE;	/* TRUE when mysql database given */
    }
verbose(1, "# testLoad options: filesToo: %s, onServer: %s, MyISAM: %s\n", filesToo ? "TRUE" : "FALSE", onServer ? "TRUE" : "FALSE", MyISAM ? "TRUE" : "FALSE");
verbose(1, "# testLoad options: mysql: '%s', testDir: '%s'\n", mysql, testDir);
verbose(1, "# testLoad options: number of files/tables: %lld, average size (bytes/rows): %lld\n", numFiles, averageSize);
/* argv[1] is fileCount, argv[2] is average size */
trashLoad(numFiles, averageSize);
if (mysqlTesting)
   cleanUp();

return 0;
}
