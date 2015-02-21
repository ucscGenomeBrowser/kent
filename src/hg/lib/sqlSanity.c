/* sqlSanity - stuff to do sanity checking on things that will go into SQL */

#include "common.h"
#include "portable.h"
#include "errAbort.h"
#include <mysql.h>
#include "dystring.h"
#include "jksql.h"
#include "sqlNum.h"
#include "sqlSanity.h"
#include "kxTok.h"


void sqlSanityCheckWhere(char *rawQuery, struct dyString *clause)
/* Let the user type in an expression that may contain
 * - field names
 * - parentheses
 * - comparison/arithmetic/logical operators
 * - numbers
 * - patterns with wildcards
 * Make sure they don't use any SQL reserved words, ;'s, etc.
 * Let SQL handle the actual parsing of nested expressions etc. -
 * this is just a token cop. 
 * The sanitized clause is in clause->string */
{
struct kxTok *tokList, *tokPtr;
char *ptr;
int numLeftParen, numRightParen;

if ((rawQuery == NULL) || (rawQuery[0] == 0))
    return;

/* tokenize (do allow wildcards, and include quotes.) */
kxTokIncludeQuotes(TRUE);
tokList = kxTokenizeFancy(rawQuery, TRUE, TRUE, TRUE);

/* to be extra conservative, wrap the whole expression in parens. */
dyStringAppend(clause, "(");
numLeftParen = numRightParen = 0;
for (tokPtr = tokList;  tokPtr != NULL;  tokPtr = tokPtr->next)
    {
    if (tokPtr->spaceBefore)
        dyStringAppendC(clause, ' ');
    if ((tokPtr->type == kxtEquals) ||
	(tokPtr->type == kxtGT) ||
	(tokPtr->type == kxtGE) ||
	(tokPtr->type == kxtLT) ||
	(tokPtr->type == kxtLE) ||
	(tokPtr->type == kxtAnd) ||
	(tokPtr->type == kxtOr) ||
	(tokPtr->type == kxtNot) ||
	(tokPtr->type == kxtAdd) ||
	(tokPtr->type == kxtSub) ||
	(tokPtr->type == kxtDiv))
	{
	dyStringAppend(clause, tokPtr->string);
	}
    else if (tokPtr->type == kxtOpenParen)
	{
	dyStringAppend(clause, tokPtr->string);
	numLeftParen++;
	}
    else if (tokPtr->type == kxtCloseParen)
	{
	dyStringAppend(clause, tokPtr->string);
	numRightParen++;
	}
    else if ((tokPtr->type == kxtWildString) ||
	     (tokPtr->type == kxtString))
	{
	char *word = cloneString(tokPtr->string);
	toUpperN(word, strlen(word));
	if (startsWith("SQL_", word) ||
	    startsWith("MYSQL_", word) ||
	    sameString("ALTER", word) ||
	    sameString("BENCHMARK", word) ||
	    sameString("CHANGE", word) ||
	    sameString("CREATE", word) ||
	    sameString("DELAY", word) ||
	    sameString("DELETE", word) ||
	    sameString("DROP", word) ||
	    sameString("FLUSH", word) ||
	    sameString("GET_LOCK", word) ||
	    sameString("GRANT", word) ||
	    sameString("INSERT", word) ||
	    sameString("KILL", word) ||
	    sameString("LOAD", word) ||
	    sameString("LOAD_FILE", word) ||
	    sameString("LOCK", word) ||
	    sameString("MODIFY", word) ||
	    sameString("PROCESS", word) ||
	    sameString("QUIT", word) ||
	    sameString("RELEASE_LOCK", word) ||
	    sameString("RELOAD", word) ||
	    sameString("REPLACE", word) ||
	    sameString("REVOKE", word) ||
	    sameString("SELECT", word) ||
	    sameString("SESSION_USER", word) ||
	    sameString("SHOW", word) ||
	    sameString("SYSTEM_USER", word) ||
	    sameString("UNLOCK", word) ||
	    sameString("UPDATE", word) ||
	    sameString("USE", word) ||
	    sameString("USER", word) ||
	    sameString("VERSION", word))
	    {
	    errAbort("Illegal SQL word \"%s\" in free-form query string",
		     tokPtr->string);
	    }
	else if (sameString("*", tokPtr->string))
	    {
	    // special case for multiplication in a wildcard world
	    dyStringPrintf(clause, "%s", tokPtr->string);
	    }
	else
	    {
	    /* Replace normal wildcard characters with SQL: */
	    while ((ptr = strchr(tokPtr->string, '?')) != NULL)
		*ptr = '_';
	    while ((ptr = strchr(tokPtr->string, '*')) != NULL)
	    *ptr = '%';
	    dyStringPrintf(clause, "%s", tokPtr->string);
	    }
	}
    else if (tokPtr->type == kxtPunct &&
	     sameString(",", tokPtr->string))
	{
	/* Don't take just any old punct, but allow comma for in-lists. */
	dyStringAppend(clause, tokPtr->string);
	}
    else if (tokPtr->type == kxtEnd)
	{
	break;
	}
    else
	{
	errAbort("Unrecognized token \"%s\" in free-form query string",
		 tokPtr->string);
	}
    }
dyStringAppend(clause, ")");

if (numLeftParen != numRightParen)
    errAbort("Unequal number of left parentheses (%d) and right parentheses (%d) in free-form query expression",
	     numLeftParen, numRightParen);

slFreeList(&tokList);
}

