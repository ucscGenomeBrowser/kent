/* hgsqladmin - Wrapper around mysqladmin using passwords in .hg.conf. */
#include "common.h"
#include "dystring.h"
#include "options.h"
#include "hgConfig.h"

static char const rcsid[] = "$Id: hgsqladmin.c,v 1.1 2005/10/03 21:34:57 heather Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsqladmin - Wrapper around mysqladmin using passwords in .hg.conf\n"
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

int hgsqladmin(int argc, char *argv[])
/* hgsqladmin - Wrapper around mysqladmin using passwords in .hg.conf. */
{
int i;
int rc;
struct dyString *command = newDyString(1024);
char *password = cfgOption("db.password");
char *user = cfgOption("db.user");
char *host = cfgOption("db.host");
dyStringPrintf(command, "mysqladmin -u %s -p%s -h%s", user, password, host);
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
// if (argc <= 1)
    // usage();
return (hgsqladmin(argc-1, argv+1));
}
