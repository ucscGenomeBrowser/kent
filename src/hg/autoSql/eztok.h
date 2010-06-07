/* eztok - simple but often effective tokenizer.  Separates
 * things into space or punctuation delimited words.  Punctuation
 * (anything non-alphanumeric) is returned as a single character
 * token.  It will handle quotes, but not allow escapes within
 * quotes. */

struct ezTok
/* A token. */
    {
    struct ezTok *next;
    char string[1];  /* Allocated at run time */
    };

struct ezTok *ezNewTok(char *string, int stringSize);
/* Allocate and initialize a new token. */

struct ezTok *ezTokenize(char *text);
/* Convert text to list of tokens. */
