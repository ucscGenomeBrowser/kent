/* pentConst - stuff to code constants for the pentium. */

#include "common.h"
#include "hash.h"
#include "pfToken.h"
#include "isx.h"
#include "pfPreamble.h"
#include "pentConst.h"

char *pentFloatOrLongLabel(char *buf, int bufSize, enum isxValType valType, 
	struct isxAddress *iad)
/* Return label associated with floating point or long constant. */
{
if (valType == ivLong)
    safef(buf, bufSize, "Q%lld", iad->val.tok->val.l);
else
    {
    char pre = (valType == ivFloat ? 'F' : 'D');
    safef(buf, bufSize, "%c%g", pre, iad->val.tok->val.x);
    subChar(buf, '-', '_');
    subChar(buf, '.', 'o');
    }
return buf;
}

static void printAsciiString(char *s, FILE *f)
/* Print constant string with escapes for assembler 
 * The surrounding quotes and terminal 0 are handled elsewhere. */
{
char c;
while ((c = *s++) != 0)
    {
    switch (c)
        {
	case '"':
	case '\\':
	   fputc('\\', f);
	   fputc(c, f);
	   break;
	default:
	   if (isprint(c))
	      fputc(c, f);
	   else
	      {
	      fputc('\\', f);
	      fprintf(f, "%o", c);
	      }
	   break;
	}
    }
}

void pentPrintInitConst(char *prefix, char *label, enum isxValType valType, 
	struct pfToken *tok, FILE *f)
/* Print out a constant initialization */
{
union pfTokVal val = tok->val;
switch (valType)
    {
    case ivByte:
	fprintf(f, "%s%s:\n", prefix, label);
	fprintf(f, "\t.byte\t%d\n", val.i);
	break;
    case ivShort:
	fprintf(f, "\t.align 1\n");
	fprintf(f, "%s%s:\n", prefix, label);
	fprintf(f, "\t.word\t%d\n", val.i);
	break;
    case ivInt:
	fprintf(f, "\t.align 2\n");
	fprintf(f, "%s%s:\n", prefix, label);
	fprintf(f, "\t.long\t%d\n", val.i);
	break;
    case ivLong:
	fprintf(f, "\t.align 3\n");
	fprintf(f, "%s%s:\n", prefix, label);
	fprintf(f, "\t.long\t%d\n", (int)(val.l&0xFFFFFFFF));
	fprintf(f, "\t.long\t%d\n", (int)(val.l>>32));
	break;
    case ivFloat:
	{
	float x = val.x;
	_pf_Int *i = (_pf_Int *)(&x);
	fprintf(f, "\t.align 2\n");
	fprintf(f, "%s%s:\n", prefix, label);
	fprintf(f, "\t.long\t%d\n", *i);
	break;
	}
    case ivDouble:
	{
	_pf_Long *l = (_pf_Long *)(&val.x);
	fprintf(f, "\t.align 3\n");
	fprintf(f, "%s%s:\n", prefix, label);
	fprintf(f, "\t.long\t%d\n", (int)(*l&0xFFFFFFFF));
	fprintf(f, "\t.long\t%d\n", (int)(*l>>32));
	break;
	}
    case ivString:
	fprintf(f, "\t.cstring\n");
	fprintf(f, "\t.ascii\t\"");
	printAsciiString(val.s, f);
	fprintf(f, "\\0\"\n");
	fprintf(f, "\t.data\n");
	break;
    default:
        internalErr();
	break;
    }
}


static void codeLocalConst(struct isxAddress *iad, struct hash *uniqHash, 
	boolean *pInText, FILE *f)
/* Print code that helps local non-int constant initialization. */
{
if (iad->adType == iadConst)
    {
    char buf[64];
    enum isxValType valType = iad->valType;
    struct hashEl *hel;
    switch (valType)
        {
	case ivFloat:
	case ivDouble:
	case ivLong:
	    pentFloatOrLongLabel(buf, sizeof(buf), valType, iad);
	    if ((hel = hashLookup(uniqHash, buf)) == NULL)
		{
		if (*pInText)
		    {
		    fprintf(f, "\t.data\n");
		    *pInText = FALSE;
		    }
		pentPrintInitConst("", buf, valType, iad->val.tok, f);
		hel = hashAdd(uniqHash, buf, NULL);
		}
	    iad->name = hel->name;
	    break;
	}
    }
}

void pentCodeLocalConsts(struct isxList *isxList, 
	struct hash *uniqHash, FILE *f)
/* Print code that helps local non-int constant initialization. 
 * for any sources in instruction. */
{
struct dlNode *node;
boolean inText = TRUE;
for (node = isxList->iList->head; !dlEnd(node); node = node->next)
    {
    struct isx *isx = node->val;
    if (isx->left)
       codeLocalConst(isx->left, uniqHash, &inText, f);
    if (isx->right)
       codeLocalConst(isx->right, uniqHash, &inText, f);
    }
if (!inText)
    fprintf(f, "\t.text\n");
}

