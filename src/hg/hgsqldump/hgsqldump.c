/* hgsqldump - Execute mysqldump using passwords from .hg.conf. */
#include "common.h"
#include "dystring.h"
#include "options.h"
#include "hgConfig.h"

static char const rcsid[] = "$Id: hgsqldump.c,v 1.2 2003/09/08 21:24:43 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsqldump - Execute mysqldump using passwords from .hg.conf\n"
  "usage:\n"
  "   hgsqldump [OPTIONS] database [tables]\n"
  "or:\n"
  "   hgsqldump [OPTIONS] --databases [OPTIONS] DB1 [DB2 DB3 ...]\n"
  "or:\n"
  "   hgsqldump [OPTIONS] --all-databases [OPTIONS]\n"
  "Generally anything in command line is passed to mysqldump\n"
  "\tafter an implicit '-u user -ppassword\n"
  "See also: mysqldump\n"
  "Note: directory for results must be writable by mysql.  i.e. 'chmod 777 .'\n"
  "Which is a security risk, so remember to change permissions back after use.\n"
  "e.g.: hgsqldump --all -c --tab=. cb1"
  );
}

boolean stringHasSpace(char *s)
/* Return TRUE if white space in string */
{
char c;
while ((c = *s++) != 0)
    {
    if (isspace(c))
        return TRUE;
    }
return FALSE;
}

void hgsqldump(int argc, char *argv[])
/* hgsqldump - Execute mysqldump using passwords from .hg.conf. */
{
int i;
struct dyString *command = newDyString(1024);
char *password = cfgOption("db.password");
char *user = cfgOption("db.user");
dyStringPrintf(command, "mysqldump -u %s -p%s", user, password);
for (i=0; i<argc; ++i)
    {
    boolean hasSpace = stringHasSpace(argv[i]);
    dyStringAppendC(command, ' ');
    if (hasSpace)
	dyStringAppendC(command, '\'');
    dyStringAppend(command, argv[i]);
    if (hasSpace)
	dyStringAppendC(command, '\'');
    }
system(command->string);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc <= 1)
    usage();
hgsqldump(argc-1, argv+1);
return 0;
}
