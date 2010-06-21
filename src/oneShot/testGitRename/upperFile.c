/* upperFile - convert file to upper case. */

#include "common.h"
#include "linefile.h"

void testGitRename(char *input, char *output)
/* testGitRename - Just a little something to test renaming, merging, etc.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    touppers(line);
    fprintf(f, "%s\n", line);
    }

carefulClose(&f);
}

