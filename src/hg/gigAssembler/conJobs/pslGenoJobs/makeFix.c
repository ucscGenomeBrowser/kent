/* One shot fix thing. */
#include "common.h"
#include "linefile.h"


int main(int argc, char *argv[])
{
char *rerunDir = "/projects/cc/hg/gs.2/g2g/g2g.fix";
FILE *f = mustOpen("/projects/cc/hg/gs.2/g2g/g2g.fix/makeFix.con", "w");
char *inName = "/projects/cc/hg/gs.2/g2g/miss.txt";
struct lineFile *lf = lineFileOpen(inName, TRUE);
int wordCount, lineSize, len;
char *line, *words[16];
char *a, *b;
char *s;
char both[32];

while (lineFileNext(lf, &line, &lineSize))
    {
    s = strchr(line, ':');
    if (s == NULL)
	continue;
    *s = 0;
    a = line;
    b = strchr(a, '_');
    if (b == NULL)
	errAbort("Expecting '_' in first word line %d of %s", lf->lineIx, lf->fileName);
    *b++ = 0;
    sprintf(both, "%s_%s", a, b);
    uglyf("Processing %s\n", both);
    fprintf(f, "log = %s/log/%s\n", rerunDir, both);
    fprintf(f, "error = %s/err/%s\n", rerunDir, both);
    fprintf(f, "output = %s/out/%s\n", rerunDir, both);
    fprintf(f, "arguments = in/%s in/%s g2g /var/tmp/hg/h/11.ooc %s/psl/%s.psl\n",
    	a, b, rerunDir, both);
    fprintf(f, "queue 1\n\n");
    }
}
