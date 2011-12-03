/* gensatFtpList - Given CHECKSUM.MD5 file with the file names munged, 
 * convert to names can actually download from ftp.ncbi.nlm.gov/pub/gensat. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "gensatFtpList - Given CHECKSUM.MD5 file with the file names munged,\n"
  "convert to names can actually download from ftp.ncbi.nlm.gov/pub/gensat\n"
  "usage:\n"
  "   gensatFtpList in.md5 out.md5\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

char *monthNames[] = 
    {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
char *monthNums[] =
    {
    "01", "02", "03", "04", "05", "06",
    "07", "08", "09", "10", "11", "12",
    };

char *fixName(char *name, struct lineFile *lf)
/* Convert somethign that looks like:
 *   StJude_2004Aug31_rep2/g00005/g00005_sj_g44_142_ads.jpg
 * to something that looks like:
 *   StJude_2004Aug31_rep2/g00005/g00005_sj_g44_142_ads.jpg
 * This will consume name, converting it in place. 
 * Some of the input names may already be in the correct format. */
{
/* Essentially all we do is look for three letter month,
 * and replace it with two letter number. */
char *month;
static char abbrMonth[4];
int monthIx;
char *firstUnder = strchr(name, '_');
char *s, c;
if (firstUnder == NULL)
    errAbort("Can't find underbar in %s", name);

month = firstUnder + 5;
if (!(isdigit(month[0]) && isdigit(month[1])))
    {
    memcpy(abbrMonth, month, 3);
    monthIx = stringIx(abbrMonth, monthNames);
    if (monthIx < 0)
	errAbort("Can't find month %s in %s", abbrMonth, name);
    else
	{
	memcpy(month, monthNums[monthIx], 2);
	strcpy(month+2, month+3);
	}
    }

/* Convert from _ to - up to first slash */
s = name;
for (;;)
    {
    c = *s;
    if (c == 0 || c == '/')
       break;
    if (c == '_')
       *s = '-';
    s += 1;
    }
return name;
} 

void gensatFtpList(char *in, char *out)
/* gensatFtpList - Given CHECKSUM.MD5 file with the file names munged, 
 * convert to names can actually download from ftp.ncbi.nlm.gov/pub/gensat. */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f = mustOpen(out, "w");
char *line, *md5;
char *oldPrefix = "/am/ftp-gensat/";
int oldPrefixSize = strlen(oldPrefix);
struct hash *suffixHash = newHash(0);

while (lineFileNext(lf, &line, NULL))
    {
    char dir[PATH_LEN], file[PATH_LEN], extension[PATH_LEN];
    md5 = nextWord(&line);
    line = skipLeadingSpaces(line);
    if (stringIn(".icon.", line))
        continue;
    if (stringIn(".med.", line))
        continue;
    if (!startsWith(oldPrefix, line))
        errAbort("Missing %s line %d of %s", oldPrefix, lf->lineIx, 
		lf->fileName);
    line += oldPrefixSize;
    if (startsWith("duplicates/", line))
        continue;
    if (endsWith(line, ".txt") || endsWith(line, ".psd")
       || endsWith(line, ".zip") || endsWith(line, ".doc")
       || endsWith(line, ".list"))
       continue;
    splitPath(line, dir, file, extension);
    hashStore(suffixHash, extension);
    fprintf(f, "%s\t%s\n", md5, fixName(line, lf));
    }

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
gensatFtpList(argv[1], argv[2]);
return 0;
}
