/* Command line main driver module for paraFlow compiler. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "hash.h"
#include "dystring.h"
#include "localmem.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfParse.h"

void usage()
/* Explain command line and exit. */
{
errAbort(
"This program compiles paraFlow code.  ParaFlow is a parallel programming\n"
"language designed by Jim Kent.\n"
"Usage:\n"
"    paraFlow input.pf\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

static struct hash *createReservedWords()
/* Create reserved words table. */
{
struct hash *hash = hashNew(7);
hashAddInt(hash, "class", pftClass);
hashAddInt(hash, "to", pftTo);
hashAddInt(hash, "para", pftPara);
hashAddInt(hash, "flow", pftFlow);
hashAddInt(hash, "into", pftInto);
hashAddInt(hash, "for", pftFor);
hashAddInt(hash, "foreach", pftForeach);
hashAddInt(hash, "while", pftWhile);
hashAddInt(hash, "of", pftOf);
hashAddInt(hash, "if", pftIf);
hashAddInt(hash, "else", pftElse);
hashAddInt(hash, "break", pftBreak);
hashAddInt(hash, "return", pftReturn);
hashAddInt(hash, "continue", pftContinue);
return hash;
}

void paraFlow(char *fileName)
/* parse and dump. */
{
struct hash *reservedWords = createReservedWords();
struct pfTokenizer *tkz = pfTokenizerNew(fileName, reservedWords);
struct pfParse *program = pfParseNew(pptProgram, NULL, NULL);
struct pfParse *pp = pfParseFile(fileName, tkz, program);
program->children = pp;
pfParseDump(program, 0, stdout);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
paraFlow(argv[1]);
return 0;
}

