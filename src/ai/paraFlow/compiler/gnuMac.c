/* gnuMac - Stuff concerned especially with code generation for
 * the Gnu Mac pentium assembler . */

#include "common.h"
#include "dlist.h"
#include "pfParse.h"
#include "pfToken.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfCompile.h"
#include "pfPreamble.h"
#include "isx.h"
#include "pentCode.h"
#include "gnuMac.h"

#define cstrPrefix "JK_"	/* Prefix before string constants */

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

static void printInitConst(enum isxValType valType, struct pfParse *initPp, 
	FILE *f)
/* Print out a constant initialization */
{
union pfTokVal val = initPp->tok->val;
switch (valType)
    {
    case ivByte:
	fprintf(f, "\t.byte\t%d\n", val.i);
	break;
    case ivShort:
	fprintf(f, "\t.align 1\n");
	fprintf(f, "\t.word\t%d\n", val.i);
	break;
    case ivInt:
	fprintf(f, "\t.align 2\n");
	fprintf(f, "\t.long\t%d\n", val.i);
	break;
    case ivLong:
	fprintf(f, "\t.align 3\n");
	fprintf(f, "\t.long\t%d\n", (int)(val.l&0xFFFFFFFF));
	fprintf(f, "\t.long\t%d\n", (int)(val.l>>32));
	break;
    case ivFloat:
	{
	float x = val.x;
	_pf_Int *i = (_pf_Int *)(&x);
	fprintf(f, "\t.align 2\n");
	fprintf(f, "\t.long\t%d\n", *i);
	break;
	}
    case ivDouble:
	{
	_pf_Long *l = (_pf_Long *)(&val.x);
	fprintf(f, "\t.align 3\n");
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

static void declareModuleVars(struct dlList *iList, FILE *f, boolean doInitted)
/* Declare module-level variables, either initted or not depending on flag. */
{
struct dlNode *node;
if (doInitted)
    fprintf(f, "\t.data\n");
for (node = iList->head; !dlEnd(node); node = node->next)
    {
    struct isx *isx = node->val;
    struct isxAddress *dest = isx->dest;
    struct pfVar *var = dest->val.var;
    struct pfParse *initPp = var->parse->children->next->next;
    enum pfAccessType access = var->ty->access;
    boolean isGlobal = (access == paGlobal || access == paWritable);
    boolean constInit = initPp != NULL;
    if (doInitted)
	{
	if (constInit)
	    {
	    if (isGlobal)
		fprintf(f, ".globl\t%s%s\n", isxPrefixC, var->cName);
	    fprintf(f, "%s%s:\n", isxPrefixC, var->cName);
	    printInitConst(dest->valType, initPp, f);
	    }
	}
    else 
	{
	if (!constInit)
	    {
	    int size = pentTypeSize(dest->valType);
	    if (isGlobal)
		{
		fprintf(f, ".align %d\n", size);
		fprintf(f, ".comm");
		fprintf(f, " %s%s,%d\n", isxPrefixC, var->cName,
			size);
		}
	    else
		{
		fprintf(f, ".lcomm");
		fprintf(f, " %s%s,%d,%d\n", isxPrefixC, var->cName,
			size, size);
		}
	    }
	}
    }
}

void gnuMacInittedModuleVars(struct dlList *iList, FILE *f)
/* Print out info on initialized variables. */
{
fprintf(f, "# Declaring initialized global and module variables\n");
declareModuleVars(iList, f, TRUE);
fprintf(f, "\n");
}

void gnuMacUninittedModuleVars(struct dlList *iList, FILE *f)
/* Print out info on uninitialized variables. */
{
fprintf(f, "# Declaring uninitialized global and module variables\n");
declareModuleVars(iList, f, FALSE);
fprintf(f, "\n");
}

void gnuMacModulePreamble(FILE *f)
/* Print out various incantations needed at start of every
 * source file for working on Mac OS X on Pentiums, or at
 * least on my mini. Also print initialized vars. */
{
fprintf(f, "# ParaFlow Pentium Output\n\n");
fprintf(f, "%s",
"# Preamble found in all modules for Mac OS-X Pentium\n"
"       .cstring\n"
"LC0:\n"
"       .ascii \"%d\\12\\0\"\n"
"       .text\n"
".globl __printInt\n"
"__printInt:\n"
"       pushl   %ebp\n"
"       movl    %esp, %ebp\n"
"       pushl   %ebx\n"
"       subl    $20, %esp\n"
"       call    ___i686.get_pc_thunk.bx\n"
"\"L00000000001$pb\":\n"
"       movl    8(%ebp), %eax\n"
"       movl    %eax, 4(%esp)\n"
"       leal    LC0-\"L00000000001$pb\"(%ebx), %eax\n"
"       movl    %eax, (%esp)\n"
"       call    L_printf$stub\n"
"       addl    $20, %esp\n"
"       popl    %ebx\n"
"       popl    %ebp\n"
"       ret\n"
"	.cstring\n"
"LC1:\n"
"	.ascii \"%f\\12\\0\"\n"
"	.text\n"
".globl __printDouble\n"
"__printDouble:\n"
"	pushl	%ebp\n"
"	movl	%esp, %ebp\n"
"	pushl	%ebx\n"
"	subl	$20, %esp\n"
"	call	___i686.get_pc_thunk.bx\n"
"\"L00000000002$pb\":\n"
"	movsd	8(%ebp), %xmm0\n"
"	movsd	%xmm0, 4(%esp)\n"
"	leal	LC1-\"L00000000002$pb\"(%ebx), %eax\n"
"	movl	%eax, (%esp)\n"
"	call	L_printf$stub\n"
"	addl	$20, %esp\n"
"	popl	%ebx\n"
"	popl	%ebp\n"
"	ret\n"

#ifdef OLD 
".globl __addTwo\n"
"__addTwo:\n"
"       pushl   %ebp\n"
"       movl    %esp, %ebp\n"
"       movl    12(%ebp), %eax\n"
"       addl    8(%ebp), %eax\n"
"       popl    %ebp\n"
"       ret\n"
#endif /* OLD */
"\n"
);
}

void gnuMacFunkyThunky(FILE *f)
/* Do that call to the funky get thunk thingie
 * that Mac does to make the code more relocatable
 * at the expense of burning the ebx register and
 * adding overhead to every single d*ng subroutine
 * almost! */
{
fprintf(f, "\tcall    ___i686.get_pc_thunk.bx\n");
}

void gnuMacModulePostscript(FILE *f)
/* Print out various incantations needed at end of every
 * source file for working on Mac OS X on Pentiums, or at
 * least on my mini. Also print uninitialized vars. */
{
fprintf(f, "%s",
"\n"
"# Postscript found in all modules for Mac OS-X Pentium\n"
"       .section __IMPORT,__jump_table,symbol_stubs,self_modifying_code+pure_instructions,5\n"
"L_printf$stub:\n"
"       .indirect_symbol _printf\n"
"       hlt ; hlt ; hlt ; hlt ; hlt\n"
"       .indirect_symbol __pfaNewString\n"
"       hlt ; hlt ; hlt ; hlt ; hlt\n"
"       .subsections_via_symbols\n"
"       .section __TEXT,__textcoal_nt,coalesced,pure_instructions\n"
".weak_definition       ___i686.get_pc_thunk.bx\n"
".private_extern        ___i686.get_pc_thunk.bx\n"
"___i686.get_pc_thunk.bx:\n"
"       movl    (%esp), %ebx\n"
"       ret\n"
);
}

void gnuMacMainStart(FILE *f)
/* Declare main function start. */
{
fprintf(f, "%s",
".globl _main\n"
"_main:\n"
"       pushl   %ebp\n"
"       movl    %esp, %ebp\n"
"       pushl   %edi\n"
"       pushl   %esi\n"
"       pushl   %ebx\n"
"       subl    $124, %esp\n"
/* NOTE: It looks like the MacTel is veeerrrry picky about
 * what the stack is at.  It seems to want to be on boundaries
 * that are even multiples of 16.  It somehow messes with the
 * 'calling helpers' they have otherwise, leading to illegal
 * instruction crashes.  I think this is all part of the
 * attempts to make the code relocatable somehow. */
);
}

void gnuMacMainEnd(FILE *f)
/* Declare main function end. */
{
fprintf(f, "%s",
"       movl    $0,%eax\n"
"       addl    $124, %esp\n"
"       popl    %ebx\n"
"       popl    %esi\n"
"       popl    %edi\n"
"       popl    %ebp\n"
"       ret\n"
);
}

