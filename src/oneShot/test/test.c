/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "memgfx.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test in \n"
  "options:\n"
  );
}

#include <termios.h>



void test(char *in)
/* test - Test something. */
{
struct termios attr;
char buf[1];

if (tcgetattr(STDIN_FILENO, &attr) != 0)
    errAbort("Couldn't do tcgetattr");
attr.c_lflag &= ~ICANON;
if (tcsetattr(STDIN_FILENO, TCSANOW, &attr) == -1)
    errAbort("Couldn't do tcsetattr");
for (;;)
   {
   int c;
   printf("Please hit the letter c\n");
   if ((c = read(0,buf,1)) < 0)
       errnoAbort("I/O error");
   if (buf[0] == 'c')
      {
      printf("Yay! got it.  Bye now\n");
      break;
      }
    printf("Hmm, I got %c.\n", buf[0]);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
test(argv[1]);
return 0;
}
