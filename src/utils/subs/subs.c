/* Subs.c - massive string substituter.  */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>


#define TRUE 1
#define FALSE 0
#define uglyf printf            /* Debugging print statement. */
typedef char Boolean;		/* By convention TRUE or FALSE */

char separator = '|';		/* Separator in subs.in file */
Boolean backup = TRUE;		/* Create backup file? */
Boolean batch = TRUE;		/* Query each replace? */
Boolean embedded = FALSE;	/* Substitute embedded subsymbols? */
Boolean writeit = TRUE;		/* Write changed  files? */


typedef struct name
/** Singly linked list of strings */
	{
	struct name *next;		/* link to next in list */
	char *name;				/* string pointer */
	} Name;

typedef struct sub
/** Structure that holds data for a single substitution to be made */
	{
	struct sub *next;		/* pointer to next substitution */
	char *in;				/* source side of substitution */
	char *out;				/* dest side of substitution */
	int insize;				/* length of in string */
	int outsize;			/* length of out string */
	} Sub;

Name *plist;				/* list of files to do subs on */


/**************** Symbol table management routines ****************/

#define HASH_ELS 256
Sub *global_hash[HASH_ELS];		/* hash table for substitutions */

int hash_function(register char *string)
/******************************************************************
 * Make up a number based on string.  A hash function should
 * try to make the number different for different strings.  Alas
 * this one only looks at the first character. (This is to
 * handle embedded case).
 ******************************************************************/
{
return (unsigned)(string[0])&(HASH_ELS-1);
}

void add_hash(Sub **table, Sub *s)
/******************************************************************
 * Attatch a single sub (s) to the list beginning at the index into
 * table determined by the in (source) element of s.
 ******************************************************************/
{
int hash;

hash = hash_function(s->in);
s->next = table[hash];
table[hash] = s;
}


Boolean first_same(register char *s, register char *t, register int len)
/******************************************************************
 * Return TRUE if  the  first len characters of the strings s and t
 * are the same.
 ******************************************************************/
{
while (--len >= 0)
	{
	if (*s++ != *t++)
		return(FALSE);
	}
return(TRUE);
}

Sub *first_in_list(register Sub *l, char *name)
/******************************************************************
 * Return first element in Sub list who's in string matches the
 * first part of name.
 ******************************************************************/
{
while (l != NULL)
	{
	if (first_same(l->in, name, l->insize))
		{
		return(l);
		}
	l = l->next;
	}
return(NULL);
}

Sub *first_in_hash(char *name)
/******************************************************************
 * Return Sub who's in string matches the first part of name
 ******************************************************************/
{
return(first_in_list(global_hash[hash_function(name)], name) );
}

int csym_len(char  *name)
/******************************************************************
 * Return the number of characters until  name is no longer a
 * C symbol.
 ******************************************************************/
{
int count = 0;
char c;

for (;;)
	{
	c = *name++;
	if (!(isalnum(c) || c == '_'))
		return(count);
	++count;
	}
}



Sub *csym_in_hash(char *name, int namelen)
/******************************************************************
 * Return Sub who's in string matches name until the bit of name
 * that couldn't be a C symbol is reached.
 ******************************************************************/
{
register Sub *l = global_hash[hash_function(name)];

while (l != NULL)
	{
	if (namelen == l->insize  && strncmp(l->in, name, namelen) == 0)
		{
		return(l);
		}
	l = l->next;
	}
return(NULL);
}

void fatal(char *txt, ...)
/******************************************************************
 * Print out a printf formatted error message and exit.
 ******************************************************************/
{
va_list args;

va_start(args,txt);
vprintf(txt,args);
va_end(args);
puts("");
exit(-1);
}

void *begmem(int size)
/******************************************************************
 * Ask for memory.  Bomb out with an error message if it's not there.
 ******************************************************************/
{
void *p;

if ((p = malloc(size)) == NULL)
	fatal("Out of memory");
return(p);
}

char *clone_string(char *n)
/******************************************************************
 * Make a duplicate of a string on the heap.
 ******************************************************************/
{
int len;
char *d;

len = strlen(n);
d = begmem(len+1);
strcpy(d,n);
return(d);
}

Boolean next_word(FILE *f, char *b, int size)
/******************************************************************
 * Put the next word (separated by white space) from file into b.
 ******************************************************************/
{
int c = 0;

/* skip leading spaces */
while (--size > 0)
	{
	c = getc(f);
	if (!isspace(c))
		break;
	}
if (c == EOF)
	return(FALSE);
/* copy non-white characters until next space */
*b++ = c;
while (--size > 0)
	{
	c = getc(f);
	if (isspace(c))
		break;
	*b++ = c;
	}
*b++ = 0;
return(TRUE);
}

void add_sub_file(char *name)
/******************************************************************
 * Add a file to the list of files to do substitutions on.
 ******************************************************************/
{
Name *n;

n = begmem(sizeof(*n));
n->name = clone_string(name);
n->next = plist;
plist = n;
}

FILE *must_open(char *name, char *how)
/******************************************************************
 * Open a file with same parameters as fopen.  Bomb out with an
 * error message if fopen fails.
 ******************************************************************/
{
FILE *f;
char buf[100];

if ((f = fopen(name, how)) == NULL)
	{
	sprintf(buf, "Can't %s %s.\n", 
		(how[0] == 'r' ? "find" : "write"), name);
	fatal(buf);
	}
return(f);
}

void sub_param_file(char *n)
/******************************************************************
 * Read in list of files to substitute on from a parameter file.
 ******************************************************************/
{
FILE *pfile;
char buf[100];

pfile = must_open(n, "r");
while (next_word(pfile, buf, sizeof(buf)))
	add_sub_file(buf);
fclose(pfile);
}

void free_names(Name *l)
/******************************************************************
 * Free up a singly linked list of names.
 ******************************************************************/
{
Name *n;

while (l != NULL)
	{
	n = l->next;
	free(l->name);
	free(l);
	l = n;
	}
}

Name *reverse_list(Name *l)
/******************************************************************
 * Reverse order of a singly linked list of names.
 ******************************************************************/
{
Name *d, *n;

d = NULL;
while (l != NULL)
	{
	n = l->next;
	l->next = d;
	d = l;
	l = n;
	}
return(d);
}

char *skip_space(char *s)
/******************************************************************
 * Return 1st non-space character in string.
 ******************************************************************/
{
while (isspace(*s))
	++s;
return(s);
}

void build_sub_file(char *sname, Boolean embed)
/******************************************************************
 * go look for lines of format :
 *		in|out
 * to tell us what to substitute
 *		in|
 * is ok if embed is false, and will delete all ins.
 *
 * if embed is true, we'll skip all white space.
 *
 ******************************************************************/
{
FILE *sfile;
int line;
char buf[256];
char *s;
char word1[256];
char word2[256];
char *d;
char c;
Sub *sub;

sfile = must_open(sname, "r");
line = 0;
for (;;)
	{
	if (fgets(buf, sizeof(buf)-1, sfile) == NULL)
		break;
	line++;
	s = buf;
	if (!embed)	
		s = skip_space(s);
	d = word1;
	for (;;)
		{
		c = *s++;
		if (c == 0)
			{
			fatal("%s %d - Line with no separator", sname, line);
			}
		if (!embed)
			{
			if (isspace(c))
				break;
			}
		else if (c == separator)
			break;
		*d++ = c;
		}
	*d++ = 0;
	d = word2;
	if (!embed)	
		s = skip_space(s);
	for (;;)
		{
		c = *s++;
		if (!embed)
			{
			if (isspace(c) || c == 0)
				break;
			}
		else
			{
			if (c == 0 || c == '\r' || c == '\n')
				break;
			}
		*d++ = c;
		}
	*d++ = 0;
	if (!embed && (word2[0] == 0))
		fatal("%s %d - Line with no substitution", sname, line);
	sub = begmem(sizeof(*sub));
	sub->in = clone_string(word1);
	sub->insize = strlen(word1);
	sub->out = clone_string(word2);
	sub->outsize = strlen(word2);
	add_hash(global_hash, sub);
	}
fclose(sfile);
}


static int llcount;
static char *ltitle;
static Boolean dirty;

/* "double" buffer for actual substitution */
static char b1[128*1024], b2[128*1024];

void report_sub(Sub *sb)
/******************************************************************
 * Print out record of the  substitution we're making.
 ******************************************************************/
{
printf("--%s %d:  %s to %s\n", ltitle, llcount, sb->in, sb->out);
}

Boolean query_sub(Sub *sb, char *orig)
/******************************************************************
 * Print out substitution and ask user if they want to do it.
 ******************************************************************/
{
char c;
Boolean answer = FALSE;
Boolean got_answer = FALSE;

report_sub(sb);
printf("%s", orig);
while (!got_answer)
	{
	printf("Yes/No/All/Quit?");
	fflush(stdout);
	for (;;)
	    {
	    c = getchar();
	    if (!isspace(c))
		break;
	    }
	switch (c)
		{
		case 'q':
		case 'Q':
			exit(0);
			break;
		case 'y':
		case 'Y':
			answer = TRUE;
			got_answer = TRUE;
			break;
		case 'n':
		case 'N':
			answer = FALSE;
			got_answer = TRUE;
			break;
		case 'A':
		case 'a':
			answer = TRUE;
			got_answer = TRUE;
			batch = TRUE;
			break;
		}
	puts("");
	}
return(answer);
}

void embedded_subline(void)
/******************************************************************
 *	This goes through and does substitutions without regard for
 *  C token conventions or anything.  Handy if you want to
 *  do a truly severe substitution that may effect embedded
 *  subsymbols.
 ******************************************************************/
{
Sub *sb;
char *s, *d;
int outdif = 0;
Boolean ldirty = FALSE;
int sizeLeft = sizeof(b1) - strlen(b1) - 1;

s = b1;
d = b2;
while (*s)
	{
	if ((sb = first_in_hash(s)) != NULL)
		{
		if (batch || query_sub(sb,b1))
			{
			outdif += (sb->outsize - sb->insize);
			if (outdif > sizeLeft) 
				fatal("Line expansion too big");
			s += sb->insize;
			memcpy(d, sb->out, sb->outsize);
			d += strlen(sb->out);
			ldirty = dirty = TRUE;
			if (batch)
				report_sub(sb);
			}
		else
			goto NOSUB;
		}
	else
		{
NOSUB:
		*d++ = *s++;
		}
	}
*d++ = 0;
if (ldirty&&batch)
	printf("%s%s", b1,b2);
}

void csym_subline(void)
/******************************************************************
 *	This does substitutions only on a full C symbol at a time.
 ******************************************************************/
{
Sub *sb;
char *s, *d;
int outdif = 0;
Boolean ldirty = FALSE;
char c;
int clen;
int sizeLeft = sizeof(b1) - strlen(b1) - 1;

s = b1;
d = b2;
while ((c = *s) != 0)
	{
	if (isalnum(c) || c== '_')
		{
		clen = csym_len(s);
		if ((sb = csym_in_hash(s,clen)) != NULL)
			{
			if (batch || query_sub(sb,b1))
				{
				outdif += (sb->outsize - sb->insize);
				if (outdif > sizeLeft)
					fatal("Line expansion too big");
				s += sb->insize;
				memcpy(d, sb->out, sb->outsize);
				d += strlen(sb->out);
				ldirty = dirty = TRUE;
				if (batch)
					report_sub(sb);
				}
			else
				goto NOSUB;
			}
		else
			{
NOSUB:
			memcpy(d,s,clen);
			d += clen;
			s += clen;
			}
		}
	else
		*d++ = *s++;
	}
*d++ = 0;
if (ldirty&&batch)
	printf("%s%s", b1,b2);
}

void backitup(char *name)
/******************************************************************
 *	Rename file.xxx to file.bak.  Overwrite old file.bak in process.
 ******************************************************************/
{
char path[512];
int len = strlen(name);
char *s;

strcpy(path, name);
s = strrchr(path, '.');
if (s == NULL)
    s = path+len;
strcpy(s, ".bak");
remove(path);
if (rename(name, path) != 0)
	{
	perror("rename");
	exit(-1);
	}
}

void sub_file(char *t)
/******************************************************************
 * Do our substitutions on a single file.
 ******************************************************************/
{
Name *ll, *n;
FILE *sfile;

sfile = must_open(t, "r");
printf("Subbing %s\n", t);
ll = NULL;
llcount = 0;
ltitle = t;
dirty = FALSE;
for (;;)
	{
	if (fgets(b1, sizeof(b1)-1, sfile) == NULL)
		break;
	if (embedded)
		embedded_subline();
	else
		csym_subline();
	n = begmem(sizeof(*n));
	n->name = clone_string(b2);
	n->next = ll;
	ll = n;
	llcount += 1;
	}
fclose(sfile);
if (dirty && writeit)
	{
	printf("saving new %s\n", t);
	ll = reverse_list(ll);
	n = ll;
	if (backup)
		backitup(t);
	sfile = must_open(t, "w");
	while (n != NULL)
		{
		fputs(n->name, sfile);
		n = n->next;
		}
	fclose(sfile);
	}
free_names(ll);
}

Boolean has_wild_parts(char *filename)
/******************************************************************
 * Returns TRUE if filename contains wildcard characters.
 ******************************************************************/
{
char c;

while ((c = *filename++) != 0)
	if (c == '*' || c == '?')
		return TRUE;
return FALSE;
}


int main(int argc, char *argv[])
/******************************************************************
 * Parse command line printing usage if it doesn't make sense.
 * Then go call subber on each file.
 ******************************************************************/
{
int i;
char *p;
int option;
char *sname = "subs.in";		/* file containing substitutions */

if (argc < 2)
	{
USAGE:
	puts("Subs - a utility to perform massive string substitutions on source");
	puts("Usage:  subs [options] file1 ... filen [options]");
	puts("Options:	-f file (get files to do subs on from file)");
	puts("          -s file (get substitutions to perform from file.");
	puts("                   by default subs looks for subs.in)");
	puts("          -r      (read only - don't write out substitutions)");
	puts("          -b      (don't create .bak files on changed files)");
	puts("          -e      (looks for embedded substrings as well as");
	puts("                   entire C symbols");
	puts("          -c char (use char as the separator in substitution");
	puts("                   file.  Only matters in embedded case.");
	puts("                   '|' by default.)");
	puts("          -i      (interactive query on each substitution.)");
	puts("The format of subs.in is oldstring<sep>newstring<cr>.");
	puts("In the normal case <sep> can be any white space and newstring");
	puts("is required.  In the embedded case <sep> defaults to '|' and");
	puts("if there is no newstring, oldstring will be eliminated.");
	puts("Hashing algorithm doesn't work for one character sub sources.");
	puts("There can be more than one substitution in the file.");
	puts("Subs does take wildcards in the list of files to substitute.");
	exit(0);
	}
for (i=1; i<argc; i++)
	{
	p = argv[i];
	if (p[0] == '-')
		{
		option = tolower(p[1]);
		switch (option)
			{
			case 'f':
				if (i >= argc-1)
					goto USAGE;
				sub_param_file(argv[++i]);
				break;
			case 's':
				if (i >= argc-1)
					goto USAGE;
				sname = argv[++i];
				break;
			case 'c':
				if (i >= argc-1)
					goto USAGE;
				separator = argv[++i][0];
				break;
			case 'b':
				backup = FALSE;
				break;
			case 'i':
				batch = FALSE;
				setvbuf(stdin, NULL, _IONBF, 0);
				break;
			case 'e':
				embedded = TRUE;
				break;
			case 'r':
				writeit = FALSE;
				break;
			default:
				goto USAGE;
			}
		}
	else if (!has_wild_parts(p))
		{
		add_sub_file(p);
		}
	}
build_sub_file(sname, embedded);
plist = reverse_list(plist);
while (plist != NULL)
	{
	sub_file(plist->name);
	plist = plist->next;
	}
return 0;
}

