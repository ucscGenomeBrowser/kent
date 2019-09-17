/* hgLoadRnaFold - Load a directory full of RNA fold files into database. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hgRelate.h"
#include "portable.h"


char *tabDir = ".";
boolean doLoad;
boolean warnEmpty;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadRnaFold - Load a directory full of RNA fold files into database\n"
  "usage:\n"
  "   hgLoadRnaFold database table foldDir\n"
  "options:\n"
  "   -tab=dir - Output tab-separated files to this directory.\n"
  "   -noLoad  - If true don't load database and don't clean up tab files\n"
  "   -warnEmpty - Warn rather than abort on empty files.\n"
  );
}

static struct optionSpec options[] = {
   {"tab", OPTION_STRING},
   {"noLoad", OPTION_BOOLEAN},
   {"warnEmpty", OPTION_BOOLEAN},
   {NULL, 0},
};

void rnaFoldCreateTable(struct sqlConnection *conn, char *table)
/* Create rnaFold table. */
{
static char *create = 
"CREATE TABLE %s (\n"
"    name varchar(255) not null, # mRNA accession\n"
"    seq longblob not null,      # mRNA sequence (U's instead of T's)\n"
"    fold longblob not null,     # Parenthesis and .'s that describe folding\n"
"    energy float not null,      # Estimated free energy of folding (negative)\n"
"              #Indices\n"
"    PRIMARY KEY(name(32))\n"
")\n";
char query[512];
sqlSafef(query, sizeof(query), create, table);
sqlRemakeTable(conn, table, query);
}

static void notFold(char *fileName, int pos)
/* Complain and die about file. */
{
errAbort("%s doesn't seen to be a RNAfold file (%d)", fileName, pos);
}

void hgLoadRnaFold(char *database, char *table, char *foldDir)
/* hgLoadRnaFold - Load a directory full of RNA fold files into database. */
{
char path[PATH_LEN];
struct slName *dirList, *dirEl;
struct lineFile *lf;
char *line, *word, *s, c;
FILE *f = hgCreateTabFile(tabDir, table);
int count = 0;

dirList = listDir(foldDir, "*");
for (dirEl = dirList; dirEl != NULL; dirEl = dirEl->next)
    {
    char *name = dirEl->name;
    if (sameString(name, "CVS"))
        continue;
    safef(path, sizeof(path), "%s/%s", foldDir, name);
    lf = lineFileOpen(path, TRUE);
    if (!lineFileNext(lf, &line, NULL))
        {
	if (warnEmpty)
	    {
	    warn("%s is empty, skipping\n", name);
	    lineFileClose(&lf);
	    continue;
	    }
	else
	    errAbort("%s is empty\n", name);
	}
    fprintf(f, "%s\t", name);	/* Save name */
    fprintf(f, "%s\t", line);	/* Save sequence */
    lineFileNeedNext(lf, &line, NULL);
    c = line[0];
    if (c != '.' && c != '(')
        notFold(path, 2);
    word = nextWord(&line);
    fprintf(f, "%s\t", word);	/* Save nested parenthesis */

    /* Parse out (energy) term at end of line. */
    s = strchr(line, '(');
    if (s == NULL)
        notFold(path, 3);
    word = skipLeadingSpaces(s+1);
    // Light error checking that first word could be a number.
    if (word == NULL || (word[0] != '-' && !isdigit(word[0])))
        notFold(path, 4);
    if ((s = strchr(word, ')')) == NULL)
        notFold(path, 5);
    *s = 0;
    fprintf(f, "%s\n", word);
    
    lineFileClose(&lf);
    ++count;
    }
printf("Parsed %d files\n", count);
if (doLoad)
    {
    struct sqlConnection *conn = sqlConnect(database);
    rnaFoldCreateTable(conn, table);
    hgLoadTabFile(conn, tabDir, table, &f);
    hgRemoveTabFile(tabDir, table);
    sqlDisconnect(&conn);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
doLoad = !optionExists("noLoad");
warnEmpty = optionExists("warnEmpty");
if (optionExists("tab"))
    {
    tabDir = optionVal("tab", tabDir);
    makeDir(tabDir);
    }
hgLoadRnaFold(argv[1], argv[2], argv[3]);
return 0;
}
