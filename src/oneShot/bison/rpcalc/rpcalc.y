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

%%
input:	/* empty */
	| input line
	;

line:	'\n'
	| exp '\n' { printf("\t %.10g\n", $1); }
	;

exp:	NUM	{$$ = $1;}
	| exp exp '+'	{$$ = $1 + $2;}
	| exp exp '-'	{$$ = $1 - $2;}
	| exp exp '*'	{$$ = $1 * $2;}
	| exp exp '/'	{$$ = $1 / $2;}
	| exp exp '^'	{$$ = pow($1,$2);}
	| exp 'n'	{$$ = -$1;}
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
     ;
/* Process numbers.  */
if (c == '.' || isdigit (c))
    {
    ungetc (c, stdin);
    scanf ("%lf", &yylval);
    return NUM;
    }
/* Return end-of-input.  */
if (c == EOF)
    return 0;
/* Return a single char.  */
return c;
}

void yyerror(char const *s)
{
fprintf(stderr, "%s\n", s);
}

int main(void)
{
printf("Enter an expression in reverse Polish notation on each line.\n"
       "Use <control>D to exit.\n");
return yyparse();
}

