/* upperFile - convert file to upper case. */

#include "common.h"
#include "linefile.h"
#include "upperFile.h"

void upperFile(char *input, char *output)
/* upperFile - convert file to upper case. */
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

