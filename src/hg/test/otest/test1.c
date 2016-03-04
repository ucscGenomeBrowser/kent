/* test - test out something. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "portable.h"
#include "jksql.h"
#include "hash.h"
#include "dystring.h"
#include "linefile.h"


#define sqlQuery sqlGetResult

char *database = "test";

void dropAll()
/* Drop all tables. */
{
struct sqlConnection *conn = sqlConnect(database);
sqlGetResult(conn, NOSQLINJ "drop table word");
sqlGetResult(conn, NOSQLINJ "drop table lineWords");
sqlGetResult(conn, NOSQLINJ "drop table lineSize");
sqlGetResult(conn, NOSQLINJ "drop table commaLine");
sqlDisconnect(&conn);
}

void createAll()
/* Define all tables. */
{
struct sqlConnection *conn = sqlConnect(database);
sqlGetResult(conn,
  NOSQLINJ "CREATE table word ("
   "id smallint not null primary key,"
   "word varchar(250) not null"
   ")" );
sqlGetResult(conn,
  NOSQLINJ "CREATE table lineSize ("
   "id int(8) not null primary key,"
   "size smallint not null"
   ")" );
sqlGetResult(conn,
  NOSQLINJ "CREATE table lineWords ("
   "line int(8) not null,"
   "word smallint not null,"
   "pos smallint not null"
   ")" );
sqlGetResult(conn,
  NOSQLINJ "CREATE table commaLine ("
   "id int(8) not null primary key,"
   "size smallint not null,"
   "wordList blob not null"
   ")" );
sqlDisconnect(&conn);
}

void freshTables()
/* Make database have defined but empty tables. */
{
dropAll();
createAll();
}

int tokSize(char *text, int size)
/* Return size of next token of text. */
{
char c;
int i;

c = text[0];
if (isspace(c))
    {
    for (i=1; i<size; ++i)
	{
	if (!isspace(text[i]))
	    break;
	}
    return i;
    }
else if (isdigit(c))
    {
    for (i=1; i<size; ++i)
	{
	if (!isdigit(text[i]))
	    break;
	}
    return i;
    }
else if (isalpha(c))
    {
    for (i=1; i<size; ++i)
	{
	if (!isalpha(text[i]))
	    break;
	}
    return i;
    }
else
    return 1;
}

void addFile(char *fileName)
/* Add all the words in file to database. */
{
struct lineFile *lf = lineFileOpen(fileName, FALSE);
char *line;
int lineSize;
char wordBuf[1024];
int lineBuf[512];
int highestWordId = 0;
int wordCount;
char *nullPt = NULL;
struct hash *wordHash = newHash(16);
struct hashEl *hel;
struct dyString *query = newDyString(512);
struct sqlConnection *conn = sqlConnect(database);
int i;
int lineCount = 0;

while (lineFileNext(lf, &line, &lineSize))
    {
    /* Chop the line up into words. */
    int wordCount = 0;
    int startWord;
    int wordSize;
    int sizeLeft;
    int wordId;
    int i;
    for (sizeLeft = lineSize; sizeLeft > 0; sizeLeft -= wordSize, line += wordSize)
	{
	wordSize = tokSize(line, sizeLeft);
	if (wordSize >= sizeof(wordBuf))
	    errAbort("Word too long line %d of %s", lf->lineIx, lf->fileName);
	memcpy(wordBuf, line, wordSize);
	wordBuf[wordSize] = 0;
	if (wordBuf[wordSize-1] == ' ')
	    wordBuf[wordSize-1] = '_';
	if ((hel = hashLookup(wordHash, wordBuf)) == NULL)
	    {
	    wordId = highestWordId++;
	    hel = hashAdd(wordHash, wordBuf, nullPt + wordId);
	    dyStringClear(query);
	    if (wordBuf[0] == '\'')
		{
		sqlDyStringPrintf(query,
		    "INSERT into word values (%d, \"'\")", wordId);
		}
	    else if (wordBuf[0] == '\\')
		{
		sqlDyStringPrintf(query,
		    "INSERT into word values (%d, '\\\\')", wordId);
		}
	    else
		{
		sqlDyStringPrintf(query,
		    "INSERT into word values (%d, '%s')", wordId, wordBuf);
		}
	    sqlGetResult(conn, query->string);
	    }
	else
	    {
	    wordId = (char *)(hel->val) - nullPt;
	    }
	if (wordCount >= ArraySize(lineBuf))
	    {
	    errAbort("Too many words in line %d of %s", lf->lineIx, lf->fileName);
	    }
	lineBuf[wordCount++] = wordId;
	}

    /* Store the words in the database */
    dyStringClear(query);
    sqlDyStringPrintf(query,
	"INSERT into commaLine values (%d, %d, '", lineCount, wordCount);
    for (i=0; i<wordCount; ++i)
	dyStringPrintf(query, "%d,", lineBuf[i]);
    dyStringAppend(query, "')");
    sqlGetResult(conn, query->string);

    dyStringClear(query);
    sqlDyStringPrintf(query,
	"INSERT into lineSize values (%d,%d)", lineCount, wordCount);
    sqlGetResult(conn, query->string);

    for (i=0; i<wordCount; ++i)
	{
	dyStringClear(query);
	sqlDyStringPrintf(query,
	    "INSERT into lineWords values (%d,%d,%d)", lineCount, lineBuf[i], i);
	sqlGetResult(conn, query->string);
	}
    ++lineCount;
    }
sqlDisconnect(&conn);
}

void usage()
{
errAbort("usage: test file\n");
}

char **loadWords()
/* Load words from table into array. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
int wordCount;
char **words = NULL;
int i = 0;
char **row;

wordCount = sqlTableSize(conn, "word");
uglyf("Got %d words\n", wordCount);
words = needMem(wordCount * sizeof(words[0]));
sr = sqlQuery(conn, NOSQLINJ "select * from word");
while ((row = sqlNextRow(sr)) != NULL)
    {
    words[i] = cloneString(row[1]);
    ++i;
    }
printf("wordCount %d i %d\n", wordCount, i);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return words;
}

void noOutput(FILE *f, char *s)
/* Write string to file (maybe) */
{
}

void fileOutput(FILE *f, char *s)
/* Write string to file (maybe) */
{
fputs(s, f);
}

void commaRecon(char *fileName)
/* Do comma based reconstruction. */
{
char **words;
long start, end;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
FILE *f = mustOpen(fileName, "w");

start = clock1000();
words = loadWords();
end = clock1000();
printf("Time to load words: %4.3f\n", 0.001*(end-start));
start = clock1000();

sr = sqlQuery(conn, NOSQLINJ "SELECT * from commaLine");
while ((row = sqlNextRow(sr)) != NULL)
    {
    int wordCount = sqlUnsigned(row[1]);
    int i;
    char *s = row[2],*e;
    int wordIx;
    for (i=0; i<wordCount; ++i)
	{
	e = strchr(s,',');
	*e++ = 0;
	wordIx = sqlUnsigned(s);
	s = e;
	fileOutput(f,words[wordIx]);
	}
    }
end = clock1000();
printf("Time to comma reconstruct file: %4.3f\n", 0.001*(end-start));
sqlDisconnect(&conn);
}

void relationRecon(char *fileName)
/* Do relationship based reconstruction. */
{
char **words;
long start, end;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
FILE *f = mustOpen(fileName, "w");
int lineCount;
int i;
char query[256];

start = clock1000();
words = loadWords();
end = clock1000();
printf("Time to load words: %4.3f\n", 0.001*(end-start));
start = clock1000();

lineCount = sqlTableSize(conn, "lineSize");
for (i=0; i<lineCount; ++i)
    {
    sqlSafef(query, sizeof query, "select * from lineWords where line = %d", i);
    sr = sqlQuery(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	fileOutput(f,words[sqlUnsigned(row[1])]);
    sqlFreeResult(&sr);
    }
end = clock1000();
printf("Time to relation reconstruct file: %4.3f\n", 0.001*(end-start));
sqlDisconnect(&conn);
}


void makeBig()
/* Make a pretty big database from a large text file. */
{
freshTables();
addFile("/usr/src/linux/Documentation/Configure.help");
}

int main(int argc, char *argv[])
{
if (argc != 2)
    usage();
relationRecon(argv[1]);
//commaRecon(argv[1]);
//makeBig();
}
