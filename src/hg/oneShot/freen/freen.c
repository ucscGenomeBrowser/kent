/* freen - My Pet Freen. */
#include <stdio.h>
#include <time.h>


int main(int argc, char *argv[])
/* Process command line. */
{
time_t now = time(NULL);
char *pt = NULL, c;
fprintf(stderr, "writing to NULL at %s\n", ctime(&now));
*pt = 0;
return 0;
}
