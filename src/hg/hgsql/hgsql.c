/* hgsql - Execute some sql code using passwords in .hg.conf. */
#include "common.h"
#include "dystring.h"
#include "options.h"
#include "hgConfig.h"

static char const rcsid[] = "$Id: hgsql.c,v 1.6 2004/07/19 18:13:32 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsql - Execute some sql code using passwords in .hg.conf\n"
  "usage:\n"
  "   hgsql database\n"
  "or:\n"
  "   hgsql database < file.sql\n"
  "or:\n"
  "   hgsql -h host database\n"
  "Generally anything in command line is passed to mysql\n"
  "after an implicit '-A -u user -ppassword"
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

int hgsql(int argc, char *argv[])
/* hgsql - Execute some sql code using passwords in .hg.conf. */
{
int i;
int rc;
struct dyString *command = newDyString(1024);
char *password = cfgOption("db.password");
char *user = cfgOption("db.user");
char *host = cfgOption("db.host");
dyStringPrintf(command, "mysql -A -u %s -p%s -h%s", user, password, host);
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
rc = system(command->string);
return (WEXITSTATUS(rc));
}

int main(int argc, char *argv[])
/* Process command line. */
{
int rc;
if (argc <= 1)
    usage();
return (hgsql(argc-1, argv+1));
}
