/* optBranch - optimize branch instructions.  This works on the
 * intermediate isx code.  It replaces jumps to jumps with
 * direct single jumps, and turns conditional jumps followed directly
 * by unconditional jumps into a single conditional jump. */

#include "common.h"
#include "hash.h"
#include "dlist.h"
#include "pfCompile.h"
#include "isx.h"
#include "optBranch.h"

static struct hash *hashLabels(struct dlList *iList)
/* Return hash of all labels. */
{
struct dlNode *node;
struct hash *hash = hashNew(0);
for (node = iList->head; !dlEnd(node); node =  node->next)
    {
    struct isx *isx = node->val;
    switch (isx->opType)
        {
	case poLabel:
	case poLoopStart:
	case poLoopEnd:
	case poCondCase:
	case poCondEnd:
	    hashAdd(hash, isx->dest->name, node);
	    break;
	}
    }
return hash;
}

static enum isxOpType invOp(enum isxOpType op)
/* Return opposite op. */
{
switch (op)
    {
    case poBeq: return poBne;
    case poBne: return poBeq;
    case poBlt: return poBge;
    case poBle: return poBgt;
    case poBgt: return poBle;
    case poBge: return poBlt;
    case poBz:  return poBnz;
    case poBnz: return poBz;
    default:
        internalErr();
	return op;
    }
}

static struct isxAddress *makeDirect(struct isx *isx, struct hash *labelHash)
/* Isx is a conditional or unconditional jump instruction.
 * If it's target is an unconditional jump, then eliminate
 * the middle man or men, and jump straight to final destination */
{
struct isxAddress *dest = isx->dest;
char *name = dest->name;
struct dlNode *node = hashMustFindVal(labelHash, name);
for (; !dlEnd(node); node = node->next)
    {
    struct isx *isx = node->val;
    switch (isx->opType)
	{
	case poLabel:
	case poLoopStart:
	case poLoopEnd:
	case poCondCase:
	case poCondStart:
	case poCondEnd:
	    break;
	case poJump:
	    // uglyf("Short circuiting %s\n", name);
	    return makeDirect(node->val, labelHash);
	default:
	    return dest;
	}
    }
return dest;
}

static boolean anyRealBefore(struct dlNode *node, struct isxAddress *label)
/* Return TRUE if get any real instruction before label */
{
for ( ; !dlEnd(node); node = node->next)
    {
    struct isx *isx = node->val;
    switch (isx->opType)
        {
	case poLabel:
	case poLoopStart:
	case poLoopEnd:
	case poCondCase:
	case poCondStart:
	case poCondEnd:
	    if (isx->dest == label)
	        return FALSE;
	    break;
	default:
	    return TRUE;
	}
    }
return TRUE;
}

static void removeNonJumps(struct dlNode *node)
/* Node is a jump.  Remove it if it just is a jump to the
 * next real (non-label) instruction */
{
struct dlNode *n;
struct isx *isx = node->val;
struct isxAddress *dest = isx->dest;
if (!anyRealBefore(node->next, dest))
    {
    // uglyf("removing non-jump to %s\n", dest->name);
    dlRemove(node);
    }
}

void optBranch(struct dlList *iList)
/* Optimize branch instructions. */
{
struct hash *labelHash = hashLabels(iList);
struct dlNode *node, *next;
for (node = iList->head; !dlEnd(node); node =  next)
    {
    struct isx *isx = node->val;
    next = node->next;
    switch (isx->opType)
        {
	case poBeq:
	case poBne:
	case poBlt:
	case poBle:
	case poBgt:
	case poBge:
	case poBz:
	case poBnz:
	    {
	    if (!dlEnd(next))
	        {
		struct isx *nextIsx = next->val;
		if (nextIsx->opType == poJump)
		    {
		    struct dlNode *nn = next->next;
		    if (!anyRealBefore(nn, isx->dest))
			{
			// uglyf("Consolidating cond/jump into !cond\n");
			isx->opType = invOp(isx->opType);
			isx->dest = nextIsx->dest;
			dlRemove(next);
			next = nn;
			}
		    }
		}
	    isx->dest = makeDirect(isx, labelHash);
	    break;
	    }
	case poJump:
	    {
	    isx->dest = makeDirect(isx, labelHash);
	    removeNonJumps(node);
	    break;
	    }
	}
    }
hashFree(&labelHash);
}
