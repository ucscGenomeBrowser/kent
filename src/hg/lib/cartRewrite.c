
#include "cartRewrite.h"

struct regexEdit cartEdit0[] = 
{
{"snp151Common=\\(pack\\|full\\|dense\\|squish\\)","superSnp=show&snp151Common_super=\\1"},
{"snp151Common=hide","snp151Common_super=hide"},
{"snp151Common\\.", "snp151Common_super."},
};

struct cartRewrite cartRewrites[] =
{
{ cartEdit0, ArraySize(cartEdit0)},
};

