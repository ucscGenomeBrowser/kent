/* mafAddQRows - Add quality data to a maf. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "qaSeq.h"
#include "maf.h"
#include "math.h"

#define FAKE_GAP_QUAL 100

struct indexEntry
  {
    char *name;		/* name of the chrom */
    off_t offset;	/* offset in the qac file */
  };

struct qData
  {
    char *name;			/* species name */
    FILE *fd;			/* file descriptor of qac file */
    boolean isSwapped;	/* is the qac file swapped? */
    struct hash *index;	/* hash mapping chrom to indexEntry and hence offset in qac file */
    struct qaSeq *qa;	/* cache to avoid reloading quality data */
  };


static void usage()
/* Explain usage and exit. */
{
  errAbort(
    "mafAddQRows - Add quality data to a maf\n"
    "usage:\n"
    "   mafAddQRows in.maf out.maf <list of species>\n"
    "   assumes species.qac, species.qdx exist\n"
  );
}

/* Buid a hash mapping chrom to file offset in a qac file for the */
/* argument species.                                              */

static struct hash *loadQacIndex(char *species)
  {
    char buffer[1024];
    FILE *qdx;
    int seqCount;
    struct hash *qacIndex;
    char seq_name[1024];
    off_t offset;
    struct hashEl *hel;
    struct indexEntry *ie;

    safef(buffer, sizeof(buffer), "%s.qdx", species);
    qdx = mustOpen(buffer, "rb");

    /* get the count of sequences in the qac file */
    if (fgets(buffer, sizeof(buffer), qdx) == NULL)
      {
        errAbort("Error reading %s quality index\n", species);
      }

    if (sscanf(buffer, "%d", &seqCount) != 1)
      {
        errAbort("Can't get sequence count from %s quality index\n", species);
      }

    qacIndex = newHash(digitsBaseTwo((unsigned long) seqCount));

    /* read in each index line */
    while (fgets(buffer, sizeof(buffer), qdx) != NULL)
      {
        if (sscanf(buffer, "%s %ld", seq_name, &offset) != 2)
          {
            errAbort("Can't parse %s quality index line:\n%s\n", species, buffer);
          }

        if ((hel = hashLookup(qacIndex, seq_name)) == NULL)
          {
            AllocVar(ie);
            ie->name = cloneString(seq_name);
            ie->offset = offset;
            hashAdd(qacIndex, ie->name, ie);
            seqCount--;
          }
      }

    if (ferror(qdx))
      {
        errAbort("Error reading %s quality index\n", species);
      }

    /* sanity check */
    if (seqCount != 0)
      {
        errAbort("%s quality index is inconsistent\n", species);
      }

    carefulClose(&qdx);

    return(qacIndex);
  }

/* Populate a qData struct for the named species. */

static struct qData *loadQData(char *species)
  {
    char buffer[1024];
    struct qData *qd;

    AllocVar(qd);
    qd->name = cloneString(species);

    safef(buffer, sizeof(buffer), "%s.qac", species);
    qd->fd = qacOpenVerify(buffer, &(qd->isSwapped));

    qd->index = loadQacIndex(species);
    qd->qa = NULL;

    return(qd);
  }

/* Build a hash keyed on species name.  Each species we are adding "q" */
/* lines for will have an entry in this hash.  The hash contains:      */
/*   1) species name [hash key]                                        */
/*   2) file descriptor for qac file (will be opened)                  */
/*   3) flag telling us if the qac file above needs to be swapped      */
/*   4) hash mapping chrom name to an indexEntry                       */
/*   5) a small 1-element cache for holding quality data               */

static struct hash *buildSpeciesHash(int argc, char *argv[])
  {
    struct hash *speciesHash;
    struct hashEl *hel;
    struct qData *qd;
    int i;

    speciesHash = newHash(digitsBaseTwo((unsigned long) argc - 3));

    for (i = 3; i < argc; i++)
      {
        /* skip over duplicate species given on the command line */
        if ((hel = hashLookup(speciesHash, argv[i])) == NULL)
          {
            qd = loadQData(argv[i]);
            hashAdd(speciesHash, qd->name, qd);
          }
      }

    return(speciesHash);
  }

/* return qData if found or NULL if not */

static struct qData *loadQualityData (struct mafComp *mc, struct hash *speciesHash)
  {
    char buffer[1024];
    char *species, *chrom;
    struct hashEl *hel;
    struct qData *qd;
    struct indexEntry *ie;

    /* separate species and chrom */
    strncpy(buffer, mc->src, sizeof(buffer));
    species = chrom = buffer;
    if ((chrom = strchr(buffer, '.')) == NULL)
      {
        errAbort("can't find chrom for %s\n", buffer);
      }
    *chrom++ = '\0';

    /* return if we don't have quality data for this species */
    if ((hel = hashLookup(speciesHash, species)) == NULL)
      {
        return(NULL);
      }

    qd = (struct qData *) hel->val;

    /* load the quality data if we need to */
    if (qd->qa == NULL || ! sameString(qd->qa->name, chrom))
      {
        if ((hel = hashLookup(qd->index, chrom)) == NULL)
          {
            //warn("Can't find %s %s in index\n", species, chrom);
            return(NULL);
          }

        ie = (struct indexEntry *) hel->val;

        if (fseeko(qd->fd, ie->offset, SEEK_SET) == (off_t) -1)
          {
            errnoAbort("fseeko() failed\n");
          }

        if (qd->qa != NULL)
          {
            qaSeqFree(&(qd->qa));
          }

        qd->qa = qacReadNext(qd->fd, qd->isSwapped);
      }

    return(qd);
  }

/* convert a q value into the appropriate character */
char mafQScore(UBYTE *q)
{
  int val;

  switch (*q)
    {
    case FAKE_GAP_QUAL:
      return('.');
      break;
    case 99:
      return('F');
      break;
    case 98:
      return('0');
      break;
    default:
      val = (int) floor((double) *q / 5);
      val = (val < 9) ? val : 9;
    }

  return('0' + val);
}


void mafAddQRows(char *inMaf, char *outMaf, struct hash *speciesHash)
/* mafAddQRows - Add quality data to a maf. */
{
  struct mafFile *mf;
  struct mafAli *ma;
  struct mafComp *mc;
  struct qData *qd;
  char *quality, *qp;
  FILE *outf = mustOpen(outMaf, "w");
  UBYTE *q;
  int inc = 1;

  mf = mafOpen(inMaf);
  mafWriteStart(outf, mf->scoring);

  while ((ma = mafNext(mf)) != NULL)
    {
      for (mc = ma->components; mc != NULL; mc = mc->next)
        {
          if (mc->size != 0 && (qd = loadQualityData(mc, speciesHash)) != NULL)
            {
              quality = cloneString(mc->text);
              qp = mc->quality = quality;

              if (mc->strand == '-')
                {
                  q = qd->qa->qa + qd->qa->size - 1 - mc->start;
                  inc = -1;
                }
              else
                {
                  q = qd->qa->qa + mc->start;
                  inc = +1;
                }

              while (*qp != '\0')
                {
                  if (*qp == '-')
                    {
                      qp++;
                      continue;
                    }
                  *qp++ = mafQScore(q);
                  q += inc;
                }
            }
        }

      mafWrite(outf, ma);
      mafAliFree(&ma);
    }

  carefulClose(&outf);
}


int main(int argc, char *argv[])
/* Process command line. */
{
  struct hash *speciesHash;

  if (argc < 4)
    {
      usage();
    }

  speciesHash = buildSpeciesHash(argc, argv);
  mafAddQRows(argv[1], argv[2], speciesHash);

  return(EXIT_SUCCESS);
}
