/* ancCheck - Check the validity of an anchor file. */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"
#include "seg.h"

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

static char const rcsid[] = "$Id: ancCheck.c,v 1.2 2008/02/18 20:42:01 rico Exp $";


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
struct segFile *sf = segOpen(inAnc);
struct segBlock *sb;
struct segComp *sc;
struct mafFile *mf = mafOpen(inMaf);
struct mafAli *ma;
struct mafComp *mc, *mcc;
int cbeg, cend, pos, idx;
int fail;

sb = segNext(sf);
ma = mafNext(mf);

while (sb != NULL && ma != NULL)
	{
	if ((sc = sb->components) == NULL)
		{
		warn("Anchor has no first component?\n");
		segWrite(stderr, sb);
		exit(EXIT_FAILURE);
		}
	if ((mc = ma->components) == NULL)
		{
		warn("MAF alignment has no first component?\n");
		mafWrite(stderr, ma);
		exit(EXIT_FAILURE);
		}

	/* Anchor is to the RIGHT if alignment block */
	if (sc->start >= (mc->start + mc->size))
		{
		mafAliFree(&ma);
		ma = mafNext(mf);
		continue;
		}

	/* Anchor is to the LEFT of alignment block */
	if ((sc->start + sc->size) <= mc->start)
		{
		segBlockFree(&sb);
		sb = segNext(sf);
		continue;
		}

	/* Anchor & alignment block overlap */
	cbeg = MAX(sc->start, mc->start);
	cend = MIN(sc->start + sc->size, mc->start + mc->size);
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
		segWrite(stdout, sb);
		mafWrite(stdout, ma);
		printf("----------\n");
		}

	/* Get the next alignment block */
	mafAliFree(&ma);
	ma = mafNext(mf);
	}

mafFileFree(&mf);
segFileFree(&sf);
}


int main(int argc, char *argv[])
{
if (argc != 3)
    usage();

ancCheck(argv[1], argv[2]);
return(EXIT_SUCCESS);
}
