/* Reverse polish notation calculator. */

%{ 
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include "calc.h"

int yylex(void);
void yyerror(char const *);
%}

%union {
    double val;	/* For returning numbers. */
    symrec *tptr;	/* for returning symbol table pointers. */
}

%token <val> NUM	/* Simple double precision number. */
%token <tptr> VAR FNCT	/* Variable and function. */
%type  <val> exp

%right '='
%left '-' '+'
%left '*' '/'
%left NEG /* negation--unary minus */
%right '^'

%%
input:	/* empty */
	| input line
	;

line:	'\n'
	| exp '\n' { printf("\t %.10g\n", $1); }
	| error '\n' {yyerrok; }
	;

exp:	NUM	{$$ = $1;}
	| VAR	{$$ = $1->value.var; }
	| VAR '=' exp	{$$ = $3; $1->value.var = $3; }
	| FNCT '(' exp ')' {$$ = (*($1->value.fnctptr))($3); }
	| exp '+' exp 	{$$ = $1 + $3;}
	| exp '-' exp 	{$$ = $1 - $3;}
	| exp '*' exp 	{$$ = $1 * $3;}
	| exp '/' exp 	
		{
		if ($3 != 0.0)
		    $$ = $1 / $3;
		else
		    {
		    $$ = 1;
		    fprintf(stderr, "%d.%d-%d.%d: division by zero",
		    	@3.first_line, @3.first_column,
			@3.last_line, @3.last_column);
		    }
		}
	| '-' exp %prec NEG {$$ = -$2;}
	| exp '^' exp 	{$$ = pow($1,$3);}
	| '(' exp ')'   {$$ = $2;}
	;
%%

/* The lexical analyzer returns a double floating point
number on the stack and the token NUM, or the numeric code
of the character read if not a number.  It skips all blanks
and tabs, and returns 0 for end-of-input.  */


int yylex (void)
{
int c;

/* Skip white space.  */
while ((c = getchar ()) == ' ' || c == '\t')
     ++yylloc.last_column;

/* Store starting location . */
yylloc.first_line = yylloc.last_line;
yylloc.first_column = yylloc.last_column;

/* Process numbers.  */
if (isdigit (c))
    {
    yylval.val = c - '0';
    ++yylloc.last_column;
    while (isdigit(c = getchar()))
        {
	++yylloc.last_column;
	yylval.val = yylval.val * 10 + c - '0';
	}
    if (c == '.')
        {
	double tens = 1.0;
	while (isdigit(c = getchar()))
	    {
	    ++yylloc.last_column;
	    tens *= 0.1;
	    yylval.val += tens * (c - '0');
	    }
	}
    ungetc (c, stdin);
    return NUM;
    }
if (isalpha(c))
    {
    symrec *s;
    static char *symbuf = 0;
    static int length = 0;
    int i;

    /* Initially make buffer long enough for 50 char name. */
    if (length == 0)
        {
	length = 50;
	symbuf = (char *)malloc(length+1);
	}
    i = 0;
    do
       {
       ++yylloc.last_column;
       if (i == length)
           {
	   length *= 2;
	   symbuf = (char *)realloc(symbuf, length+1);
	   }
	symbuf[i++] = c;
	c = getchar();
	}
    while (isalnum(c));
    ungetc(c, stdin);
    symbuf[i] = 0;

    s = getsym(symbuf);
    if (s == NULL)
        s = putsym(symbuf, VAR);
    yylval.tptr = s;
    return s->type;
    }

/* Return end-of-input.  */
if (c == EOF)
    return 0;

/* Handle single character stuff including location. */
if (c == '\n')
    {
    ++yylloc.last_line;
    yylloc.last_column = 0;
    }
else
    ++yylloc.last_column;
return c;
}

void yyerror(char const *s)
{
fprintf(stderr, "%s\n", s);
}

struct builtinFunction
    {
    char const *fname;
    double (*fnct)(double);
    };

struct builtinFunction const arith_fncts[] =
    {
    { "sin", sin, },
    { "cos", cos, },
    { "tan", tan, },
    { "asin", asin,},
    { "atan", atan, },
    { "ln", log, },
    { "exp", exp, },
    { "sqrt", sqrt, },
    { 0, 0, },
    };

void init_table()
/* Put arithmetic functions in table. */
{
int i;
symrec *ptr;
for (i=0; arith_fncts[i].fname != 0; i++)
    {
    ptr = putsym(arith_fncts[i].fname, FNCT);
    ptr->value.fnctptr = arith_fncts[i].fnct;
    }
ptr = putsym("pi", VAR);
ptr->value.var = 2*asin(1);
ptr = putsym("e", VAR);
ptr->value.var = exp(1);
}

int main(void)
{
yylloc.first_line = yylloc.last_line = 1;
yylloc.first_column = yylloc.last_column = 0;
init_table();
return yyparse();
}

