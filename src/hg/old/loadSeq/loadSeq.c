/* Load sequence into database. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "jksql.h"

char *database = "hgap";

char *filesTable =
    "create table files ( "
      "name varchar(64) primary key not null,"
      "path varchar(255) not null)";

/* Members of this are currently:
      mrna.fa, est.fa bacEnds.fa fin.fa unfin.fa
 */

char *seqTable =
    "create table seq ( "
      "name varchar(32) not null,"
      "file varchar(64) not null,"
      "offset bigint not null,"
      "size int not null)";

void resetDatabase()
{
struct sqlConnection *conn = sqlConnect("hgap");
// sqlGetResult(conn, filesTable);
sqlGetResult(conn, seqTable);
sqlDisconnect(&conn);
}

void usage()
/* Print usage instructions and exit. */
{
errAbort(
"loadSeq - Help load sequence data into hgap mySQL database\n"
"usage:\n"
"    loadSeq delete\n"
"This deletes the existing sequence table and resets it.\n"
"    loadSeq new\n"
"This creates a new sequence table.\n"
"    loadSeq addFile faFile symFileName output.tab\n"
"This makes a tab-delimited file for loading references to all sequences\n"
"in faFile into database.  symFileName is the symbolic name for the faFile\n"
"inside the database.\n"
"    loadSeq addDir faDir symFileName output.tab\n"
"This makes a tab-delimited file for loading references to all the sequences\n"
"in the directory full of faFiles into the database.\n");
}

void tabFileFromBigFile(char *inName, char *faSymbolicName, char *outName)
/* Make a tab delimited file of offsets from big file */
{
struct lineFile *lf;
FILE *out;
char *line;
int lineSize;
char accession[32];
long offset;
long newOffset;
boolean firstTime = TRUE;
int seqCount = 0;
int modMax = 50000;
int mod = modMax;

lf = lineFileOpen(inName, TRUE);
out = mustOpen(outName, "w");

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>')
	{
	char *s,*e;
	++seqCount;
	if (--mod == 0)
	    {
	    printf("processed %d\n", seqCount);
	    mod = modMax;
	    }
	newOffset = lf->bufOffsetInFile + lf->lineStart;
	if (!firstTime)
	    {
	    fprintf(out, "%s\t%s\t%ld\t%ld\n",
	    	accession, faSymbolicName, offset, newOffset-offset);
	    }
 	else
	    firstTime = FALSE;
	s = line+1;
	e = skipToSpaces(s);
	if (e != NULL)
	    {
	    if (e-s > sizeof(accession)-1)
		errAbort("Accession too long line %d of %s\n", lf->lineIx, inName);
	    *e = 0;
	    }
	if (s[0] == 0)
	    {
	    errAbort("No accession line %d of %s\n", lf->lineIx, inName);
	    }
	strcpy(accession, s);
	offset = newOffset;
	}
    }
lineFileClose(&lf);
newOffset = fileSize(inName);
fprintf(out, "%s\t%s\t%d\t%d\n",
    accession, faSymbolicName, offset, newOffset-offset);
printf("Got %d sequences in %s\n", seqCount, inName);
fclose(out);
}
    
void tabFileFromDir(char *dirName, char *dirSymbolicName, char *outName)
/* Make a tab delimited file of offsets from directory */
{
struct slName *dirList = listDir(dirName, "*.fa");
struct slName *fileName;
FILE *out = mustOpen(outName, "w");

printf("%d fa files in %s\n", slCount(dirList), dirName);
if (chdir(dirName) < 0)
    errAbort("Couldn't change dir to %s\n", dirName);

for (fileName = dirList; fileName != NULL; fileName = fileName->next)
    {
    char *s = fileName->name;
    long size = fileSize(s);
    char *e = strrchr(s, '.');
    *e = 0;	/* Cut off suffix. */
    fprintf(out, "%s\t%s\t0\t%ld\n",  s, dirSymbolicName, size);
    }
fclose(out);
}

int main(int argc, char *argv[])
{
char *command;

if (argc < 2)
    usage();
command = argv[1];
if (sameWord(command, "delete"))
    {
    struct sqlConnection *conn = sqlConnect("hgap");
    sqlGetResult(conn, "drop table seq");
    sqlGetResult(conn, seqTable);
    sqlDisconnect(&conn);
    }
else if (sameWord(command, "new"))
    {
    struct sqlConnection *conn = sqlConnect("hgap");
    sqlGetResult(conn, seqTable);
    sqlDisconnect(&conn);
    }
else if (sameWord(command, "addFile"))
    {
    if (argc != 5)
	usage();
    tabFileFromBigFile(argv[2], argv[3], argv[4]);
    }
else if (sameWord(command, "addDir"))
    {
    if (argc != 5)
	usage();
    tabFileFromDir(argv[2], argv[3], argv[4]);
    }
}
