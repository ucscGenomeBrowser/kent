#include <stdio.h>


void backupFile(char *fileName, char *backFileName)
/* Move file */
{
strcpy(backFileName, fileName);
strcat(backFileName, ".bak");
remove(backFileName);
if (rename(fileName, backFileName) != 0)
    {
    perror("rename");
    exit(-2);
    }
return;
}

int main(int argc, char *argv[])
{
char backFileName[512];
char *fileName;
int i;
FILE *in, *out;
char buf[1024*16];
int lineSize;

if (argc < 2)
    {
    printf("fixcr - removes trailing carraige returns from files.\n");
    printf("usage:\n");
    printf("     fixcr file(s)\n");
    exit(-1);
    }
for (i=1; i<argc; ++i)
    {
    fileName = argv[i];
    backupFile(fileName, backFileName);
    if ((in = fopen(backFileName, "r")) == NULL)
	{
	fprintf(stderr, "can't open %s\n", backFileName);
	exit(-1);
	}
    if ((out = fopen(fileName, "w")) == NULL)
	{
	fprintf(stderr, "can't open new %s\n", fileName);
	exit(-2);
	}
    while (fgets(buf, sizeof(buf), in) != NULL)
	{
	lineSize = strlen(buf);
	if (buf[lineSize-2] == '\r' && buf[lineSize-1] == '\n')
	    {
	    buf[lineSize-2] = '\n';
	    buf[lineSize-1] = 0;
	    }
	fputs(buf, out);
	}
    fclose(in);
    fclose(out);
    }
return 0;
}
