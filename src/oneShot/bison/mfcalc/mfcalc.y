/* Reverse polish notation calculator. */

%{ 
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include "calc.h"
#include "yy.h"

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

