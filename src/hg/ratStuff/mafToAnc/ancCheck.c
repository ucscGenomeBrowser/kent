/* ancCheck - Check the validity of an anchor file. */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"
#include "anc.h"

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

static char const rcsid[] = "$Id: ancCheck.c,v 1.1 2008/02/12 18:28:32 rico Exp $";


void usage()
{
errAbort(
  "ancCheck - Check the validity of an anchor file\n"
  "usage:\n"
  "   ancCheck in.anc in.maf\n"
  );
}


static void ancCheck(char *inAnc, char *inMaf)
/* Check an anchor file against the maf file used to generate it. */
{
struct ancFile *af = ancOpen(inAnc);
struct ancBlock *ab;
struct ancComp *ac;
struct mafFile *mf = mafOpen(inMaf);
struct mafAli *ma;
struct mafComp *mc, *mcc;
int cbeg, cend, pos, idx;
int fail;

ab = ancNext(af);
ma = mafNext(mf);

while (ab != NULL && ma != NULL)
	{
	if ((ac = ab->components) == NULL)
		{
		warn("Anchor has no first component?\n");
		ancWrite(stderr, ab);
		exit(EXIT_FAILURE);
		}
	if ((mc = ma->components) == NULL)
		{
		warn("MAF alignment has no first component?\n");
		mafWrite(stderr, ma);
		exit(EXIT_FAILURE);
		}

	/* Anchor is to the RIGHT if alignment block */
	if (ac->start >= (mc->start + mc->size))
		{
		mafAliFree(&ma);
		ma = mafNext(mf);
		continue;
		}

	/* Anchor is to the LEFT of alignment block */
	if ((ac->start + ab->ancLen) <= mc->start)
		{
		ancBlockFree(&ab);
		ab = ancNext(af);
		continue;
		}

	/* Anchor & alignment block overlap */
	cbeg = MAX(ac->start, mc->start);
	cend = MIN(ac->start + ab->ancLen, mc->start + mc->size);
	pos  = mc->start;
	idx  = 0;

	/* Get to position cbeg */
	while (pos < cbeg)
		{
		if (mc->text[idx] != '-')
			{
			pos++;
			}
		idx++;
		}

	/* Skip over any gaps */
	while (mc->text[idx] == '-')
		{
		idx++;
		}

	fail = 0;
	while (pos < cend)
		{
		for (mcc = ma->components; mcc != NULL; mcc = mcc->next)
			{
			/* Skip over components that represent "e" lines. */
			if ((mcc->size != 0) || (! mcc->leftStatus))
				{
				if (mcc->text[idx] == '-')
					{
					fail = 1;
					break;
					}
				}
			}
		if (fail)
			{
			break;
			}
		idx++;
		pos++;
		}

	if (fail)
		{
		printf("fail at pos = %d :: idx = %d cbeg = %d :: cend = %d\n", pos, idx, cbeg, cend);
		ancWrite(stdout, ab);
		mafWrite(stdout, ma);
		printf("----------\n");
		}

	/* Get the next alignment block */
	mafAliFree(&ma);
	ma = mafNext(mf);
	}

mafFileFree(&mf);
ancFileFree(&af);
}


int main(int argc, char *argv[])
{
if (argc != 3)
    usage();

ancCheck(argv[1], argv[2]);
return(EXIT_SUCCESS);
}
