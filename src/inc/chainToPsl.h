/* chainToPsl - convert between chains and psl.  Both of these
 * are alignment formats that can handle gaps in both strands
 * and do not include the sequence itself. */

#ifndef CHAINTOPSL_H
#define CHAINTOPSL_H

#ifndef PSL_H
#include "psl.h"
#endif

#ifndef CHAINBLOCK_H
#include "chainBlock.h"
#endif

struct psl *chainToPsl(struct chain *chain);
/* chainToPsl - convert chain to psl.  This does not fill in
 * all the fields perfectly,  but it does get the starts
 * and sizes right. */

#endif /* CHAINTOPSL_H */

