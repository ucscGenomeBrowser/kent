/* countLines - Count lines in files. */
#include <stdio.h>

void usage()
/* Explain usage and exit. */
{
fprintf(stderr,
  "cl - Count lines in files\n"
  "cl:\n"
  "   countLines file[s]\n"
  );
exit(1);
}

void countFile(char *file)
/* Count lines in one file. */
{
FILE *f = fopen(file, "r");
char buf[1024];
int count = 0;
while (fgets(buf, sizeof(buf), f) != NULL)
    ++count;
fclose(f);
printf("total: %ld\n", count);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
countFile(argv[1]);
return 0;
}
