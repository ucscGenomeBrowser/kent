/* Load up a GCG file. */

#include "common.h"
#include "dystring.h"
#include "hash.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "dnaLoad.h"
#include "options.h"
#include "linefile.h"
#include "bed.h"
#include "cutter.h"

char *tableName = "cutters";
struct slName *includes = NULL;
struct slName *excludes = NULL;

static struct optionSpec options[] = {
    {"tableName", OPTION_STRING},
    {"overrides", OPTION_STRING},
    {"justPrimary", OPTION_BOOLEAN},
    {"createOnly", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage() 
{
errAbort("hgCutters - Load REBASE restriction enzymes GCG file into database.\n"
	 "usage:\n"
	 "   hgCutters [options] database file.gcg\n"
	 "options:\n"
	 "    -createOnly      Just make the table without any rows.\n"
	 "    -justPrimary     Just use the ones without semicolons in front (the primary ones).\n"
	 "    -overrides=file  Include a file listing overrides to the set behavior.  The file\n"
	 "                      is formatted like:\n"
	 "                         include enzyme\n"
	 "                         exclude enzyme\n"
	 "                      etc.\n"
	 "    -tableName=name  Choose a different name for the loaded table.  Default is \"cutters\".");
}

void cutterTableCreate(struct sqlConnection *conn)
/* Create a table. */
{
static char *createString = 
    "CREATE TABLE %s (\n"
    "    name varchar(255) not null,	# Name of Enzyme\n"
    "    size int unsigned not null,	# Size of recognition sequence\n"
    "    matchSize int unsigned not null,	# size without Ns\n"
    "    seq varchar(255) not null,	# Recognition sequence\n"
    "    cut int unsigned not null,	# Cut site on the plus strand\n"
    "    overhang int not null,	# Overhang\n"
    "    palindromic tinyint unsigned not null,	# 1 if it's panlidromic, 0 if not.\n"
    "    semicolon tinyint unsigned not null,	# 1 if it's from a REBASE record that has a semicolon in front, 0 if not.\n"
    "    numSciz int unsigned not null,	# Number of isoscizomers\n"
    "    scizs longblob not null,	# Names of isosizomers\n"
    "    numCompanies int unsigned not null,	# Number of companies selling this enzyme\n"
    "    companies longblob not null,	# Company letters\n"
    "    numRefs int unsigned not null,	# Number of references\n"
    "    refs longblob not null	# Reference numbers\n"
    ")\n";
struct dyString *dy = newDyString(1024);
sqlDyStringPrintf(dy, createString, tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}

void loadOverrides(char *overrides)
/* Load overrides from file into the two global slName lists. */
{
struct lineFile *lf = lineFileOpen(overrides,TRUE);
char *words[2];

while (lineFileChop(lf,words))
    {
    if (sameWord(words[0],"include"))
	slNameStore(&includes, words[1]);
    else if (sameWord(words[0],"exclude"))
	slNameStore(&excludes, words[1]);
    else
	errAbort("There's a problem with the overrides file.");
    }
}

void hgLoadCutters(char *database, struct cutter *cutters)
/* Load table into database. */
{
struct cutter *one;
struct sqlConnection *conn = sqlConnect(database);
cutterTableCreate(conn);

if (!optionExists("createOnly"))
    for (one = cutters; one != NULL; one = one->next)
    {
    cutterSaveToDb(conn, one, tableName, 1024);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* The program */
{
struct cutter *cutters = NULL;
char *overrides = NULL;

optionInit(&argc, argv, options);
if (argc != 3) 
    usage();
cutters = readGcg(argv[2]);
overrides = optionVal("overrides", NULL);
if (overrides)
    loadOverrides(overrides);
cullCutters(&cutters, !optionExists("justPrimary"), includes, 0);
hgLoadCutters(argv[1], cutters);
cutterFreeList(&cutters);
slFreeList(&includes);
slFreeList(&excludes);
return 0;
}
