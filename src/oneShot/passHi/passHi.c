/* Write to standard output all lines above threshold. */
#include "common.h"

void usage()
/* Print usage instructions and exit. */
{
errAbort("passHi - write all lines of input to standard output\n"
	 "that start with a number higher than threshold\n"
	 "Usage:\n"
	 "   passHi threshold\n"
	 "Takes input from standard in\n"
	 "   passHi threshold file(s)\n"
	 "takes input from file(s).");
}

void filter(FILE *f, double threshold)
/* Copy lines from f to stdOut if above threshold. */
{
char line[2*1024];
int lineCount = 0;
double d;
char *s, *p;

while (fgets(line, sizeof(line), f))
    {
    s = skipLeadingSpaces(line);
    p = strchr(s, '%');
    if (p != NULL && isdigit(s[0]))
	{
	d = atof(s);
	if (d >= threshold && d <= 100.001)
	    {
	    fputs(line, stdout);
	    }
	}
    }
}


int main(int argc, char *argv[])
{
FILE *f;
int i;
double threshold;

if (argc < 2 || !isdigit(argv[1][0]))
    usage();
threshold = atof(argv[1]);
if (argc == 2)
    filter(stdin, threshold);
for (i = 2; i<argc; ++i)
    {
    f = mustOpen(argv[i], "r");
    filter(f, threshold);
    fclose(f);
    }
}
