/* hgsql - Execute some sql code using passwords in .hg.conf. */
#include "common.h"
#include "dystring.h"
#include "options.h"
#include "hgConfig.h"

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

void hgsql(int argc, char *argv[])
/* hgsql - Execute some sql code using passwords in .hg.conf. */
{
int i;
struct dyString *command = newDyString(1024);
char *password = cfgOption("db.password");
char *user = cfgOption("db.user");
dyStringPrintf(command, "mysql -A -u %s -p%s", user, password);
for (i=0; i<argc; ++i)
    dyStringPrintf(command, " %s", argv[i]);
system(command->string);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc <= 1)
    usage();
hgsql(argc-1, argv+1);
return 0;
}
