/* wabToSt - Convert WABA output to something Intronerator understands better. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wabToSt - Convert WABA output to something Intronerator understands better\n"
  "usage:\n"
  "   wabToSt out.st in1.wab ... inN.wab\n");
}

void copyLine(FILE *s, FILE *d)
/* Copy the next line of s to d. */
{
int c;

for (;;)
    {
    c = fgetc(s);
    if (c == -1)
        break;
    fputc(c, d);
    if (c == '\n')
        break;
    }
if (ferror(d))
    {
    perror("Error writing file\n");
    errAbort("\n");
    }
}

void wabToSt(char *outName, int inCount, char *inNames[])
/* wabToSt - Convert WABA output to something Intronerator understands better. */
{
FILE *in, *out = mustOpen(outName, "w");
int i, len, wordCount, partCount;
char *inName;
char buf[1024], *row[16], *parts[4];
char *s;
int lineCount = 0;
char dir[256], file[128], ext[64];
char dir2[256], file2[128], ext2[64];

for (i=0; i<inCount; ++i)
    {
    inName = inNames[i];
    in = mustOpen(inName, "r");
    lineCount = 0;
    for (;;)
        {
	/* Parse header line and print first fields unchanged. */
	if (fgets(buf, sizeof(buf)-1, in) == NULL)
	    break;
	++lineCount;
	wordCount = chopLine(buf, row);
	if (wordCount != 10)
	    errAbort("Expecting 10 words line %d of %s", lineCount, inName);
	fprintf(out, "%s %s %s %s %s ",
	   row[0], row[1], row[2], row[3], row[4]);

	/* Chop off all but last directory of briggsae file and print. */
	splitPath(row[5], dir, file, ext);
	len = strlen(dir);
	if (len > 0)
	    dir[len-1] = 0;
	splitPath(dir, dir2, file2, ext2);
	fprintf(out, "%s%s/%s:", file2, ext2, file);

	/* Print out range in briggsae file. */
	partCount = chopString(row[6], ":-", parts, ArraySize(parts));
	if (partCount != 3)
	    errAbort("Expecting 3 parts line %d of %s", lineCount, inName);
	fprintf(out, "%s-%s ", parts[1], parts[2]);

	/* Print out strand. */
	fprintf(out, "%s ", row[7]);

	/* Parse out chromosome bit. */
	partCount = chopString(row[8], ":-", parts, ArraySize(parts));
	if (partCount != 3)
	    errAbort("Expecting 3 parts in line %d of %s", lineCount, inName);
	s = strrchr(parts[0], '_');
	if (s == NULL)
	    s = strrchr(parts[0], '.');
	if (s != NULL)
	    ++s;
	else
	    s = parts[0];
	tolowers(s);
	fprintf(out, "%s:%s-%s ", s, parts[1], parts[2]);

	/* Print out strand. */
	fprintf(out, "%s\n", row[9]);

	/* Print out other three lines in group. */
	copyLine(in, out);
	copyLine(in, out);
	copyLine(in, out);
	}
    carefulClose(&in);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
wabToSt(argv[1], argc-2, argv+2);
return 0;
}
