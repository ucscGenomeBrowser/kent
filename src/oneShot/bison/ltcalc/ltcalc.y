/* Reverse polish notation calculator. */

%{ 
#define YYSTYPE double
#include <math.h>
#include <ctype.h>
#include <stdio.h>
int yylex(void);
void yyerror(char const *);
%}

%token NUM
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
    yylval = c - '0';
    ++yylloc.last_column;
    while (isdigit(c = getchar()))
        {
	++yylloc.last_column;
	yylval = yylval * 10 + c - '0';
	}
    if (c == '.')
        {
	double tens = 1.0;
	while (isdigit(c = getchar()))
	    {
	    ++yylloc.last_column;
	    tens *= 0.1;
	    yylval += tens * (c - '0');
	    }
	}
    ungetc (c, stdin);
    return NUM;
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

int main(void)
{
yylloc.first_line = yylloc.last_line = 1;
yylloc.first_column = yylloc.last_column = 0;
return yyparse();
}

