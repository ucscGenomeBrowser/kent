/* encodeCvTerm - print the RA stanza for an ENCODE Controlled Vocabulary term */

#include "common.h"
#include "dystring.h"
#include "cv.h"
#include "hash.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodeCvTerm - print the .ra stanza for the selected ENCODE CV term\n"
  "usage:\n"
  "   encodeCvTerm <type> <term> [<field>]\n"
  );
}

void encodeCvTerm(char *type, char *term, char *field)
/* print CV term ra*/
{
struct hash *termHash = (struct hash *) cvOneTermHash(type, term);
char *ret;
if (!termHash)
    errAbort("Can't find term '%s' of type '%s' in CV", term, type);
if (field)
    {
    ret = hashFindVal(termHash, field);
    if (!ret)
        errAbort("Can't find field '%s' in term '%s' of type '%s' in CV", field, term, type);
    }
else
    ret = hashToRaString(termHash);
puts(ret);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();

char *type = argv[1];
char *term = argv[2];
char *field = NULL;
if (argc == 4)
    field = argv[3];
encodeCvTerm(type, term, field);
return 0;
}
