/* Jim grep - crude approximation of grep. */
#include "common.h"

int main(int argc, char *argv[])
{
char line[4*1024];
char *fileName, *pattern;
FILE *f;
int i;

if (argc < 3)
    {
    errAbort("jimgrep - print lines of file that include pattern\n"
             "usage:\n"
             "    jimgrep pattern file(s)");
    }
pattern = argv[1];
for (i=2; i<argc; ++i)
	{
	fileName = argv[i];
	printf("Searching %s for %s\n", fileName, pattern);
	f = mustOpen(fileName, "r");
	while (fgets(line, sizeof(line), f))
		{
		if (strstr(line, pattern) != NULL)
			fputs(line, stdout);
		}
	fclose(f);
	}
return 0;
}