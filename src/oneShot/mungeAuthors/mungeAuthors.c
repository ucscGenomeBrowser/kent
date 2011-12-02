/* mungeAuthors - Change format of author list. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mungeAuthors - Change format of author list\n"
  "usage:\n"
  "   mungeAuthors XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void mungeAuthors(char *inFile)
/* mungeAuthors - Change format of author list. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *line, *s, *e;
char *lastName, *initials;

while (lineFileNextReal(lf, &line))
    {
    s = line;
    while (s != NULL && s[0] != 0)
        {
	s = skipLeadingSpaces(s);
	e = strchr(s, ',');
	if (e == NULL)
	    e = strchr(s, '.');
	if (e != NULL)
	    *e++ = 0;
	lastName = s;
	initials = strrchr(s, ' ');
	if (initials != NULL)
	    *initials++ = 0;
	printf("%s", lastName);
	if (initials != NULL)
	    {
	    if (strlen(initials) <= 3)
	        {
		char c;
		printf(",");
		while ((c = *initials++) != 0)
		    printf(" %c.", c);
		}
	    else
	        printf(" %s", initials);
	    } 
	printf("\n");
	s = e; 
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
mungeAuthors(argv[1]);
return 0;
}
