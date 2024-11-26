
#include "common.h"
#include "cart.h"

void cartEdit5(struct cart *cart)
{
cartMatchValue(cart, "gnomadVariantsV4", "gnomadVariantsV4.1");
cartMatchValue(cart, "gnomadExomesV4", "gnomadExomesVariantsV4_1");
cartMatchValue(cart, "gnomadGenomesV4", "gnomadGenomesVariantsV4_1");
cartMatchValue(cart, "gnomadExomesV4_sel", "gnomadExomesVariantsV4_1_sel");
cartMatchValue(cart, "gnomadGenomesV4_sel", "gnomadGenomesVariantsV4_1_sel");
}

