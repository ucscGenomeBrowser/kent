/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "xp.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test a \n"
  "options:\n"
  );
}

struct refString
    {
    int refCount;
    int type;
    char *s;
    int size;
    int alloced;
    bool isConst;
    };

struct refString refString = {0, 0, "Hello world", 11, 11, TRUE};

void test(char *word, char *line)
{
boolean isSame = startsWithWord(word, line);
printf("'%s' %s first word of '%s.'\n", word,
    (isSame ? "is" : "isn't"), line);
}

int main(int argc, char *argv[], char *env[])
/* Process command line. */
{
int i;
if (argc != 3)
   usage();
test(argv[1], argv[2]);
return 0;
}
