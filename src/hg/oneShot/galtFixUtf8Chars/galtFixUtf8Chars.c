/* galtFixUtf8Chars - Find database table fields with utf-8 values. Optionally fix by re-encoding to windows-1252 or iso-8859-1. */
#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "obscure.h"
#include "options.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "hex.h"
#include "hash.h"

boolean fix = FALSE;
boolean useHtmlEntities = FALSE;
boolean cgiDecodeValues = FALSE;
struct hash *hash = NULL;
struct hash *dupeVals = NULL;
int dupeCount = 0;

int unmappedCharCount = 0;

void usage()
/* Explain usage and exit. */ {
errAbort(
  "galtFixUtf8Chars - Find database table fields with utf-8 values. Optionally fix by re-encoding to windows1252 (superset of iso-8859-1).\n"
  "usage:\n"
  "   galtFixUtf8Chars find profile database table field\n"
  "       does a dry run looking for valid utf-8 values in the specified table.field using profile \n"
  "   galtFixUtf8Chars fix profile database table field\n"
  "       replaces valid utf-8 values with correct windows-1252 values in the specified table.field using profile \n"
  "options:\n"
  "   -useHtmlEntities causes values not mappable to windows-1252 to be converted to unicode codepoint entities.\n"
  "   -cgiDecode runs cgiDecode on values in db field before further processing.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"useHtmlEntities", OPTION_BOOLEAN},
   {"cgiDecode", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean hasAsciiCharsOnly(char *s)
/* does string s have only ASCII chars, i.e. codepoints 0-127 */
{
char c = 0;
while ((c = *s++))
    {
    if (c < 0)
       return FALSE;	
    }
return TRUE;
}

int countUtf8WidthInBytes(char *s) 
/* Length in bytes of a single multi-byte utf8 char.
 * Only feed valid utf8 sequences to this function. */
{
char c = *s;
int bytes = 0;
while ((c & 0x80) == 0x80)
    {
    ++bytes;
    c <<= 1;
    }
// regular non-utf8 single-byte char in codepoint range 0-127.
if (bytes == 0)
    bytes = 1;
else
    {
    if (bytes < 2)
	errAbort("remainder of utf8 should have at least two bytes");
    if (bytes > 4) 
	errAbort("remainder of utf8 should have no more than four bytes");
    }
return bytes;
}

unsigned int utf8CodepointExt(char *s, int *retBits, int *retBytes)
/* Return the unicode codepoint for a valid utf8 multi-byte character */
{
unsigned char c = 0;
unsigned char d = 0;
int remaining = 0;
unsigned int codePoint = 0;
int bytes = 0;
int bits = 0;
int thisBits = 0;
c = *s++;
//verbose(1, "c=%c=%02x\n", c, (unsigned)c);  // DEBUG REMOVE
remaining = -1;
d = c;
while ((d & 0x80) == 0x80)
    {
    ++remaining;
    d <<= 1;
    }
bytes = remaining + 1;
//verbose(1, "remaining=%d bytes=%d\n", remaining, bytes);  // DEBUG REMOVE
if (bytes == 0)  // Single ASCII char
    {
    bytes = 1;
    remaining = 0;
    thisBits = 8;  // byte is 8
    }
else
    {
    if (bytes < 2)
	errAbort("remainder of utf8 should have at least two bytes");
    if (bytes > 4) 
	errAbort("remainder of utf8 should have no more than four bytes");
    thisBits = 8 - bytes - 1;  // byte is 8, bytes = bits used, -1 for 0 stop bit.
    }
while (TRUE)
    {
//    verbose(1, "thisBits=%d\n", thisBits);  // DEBUG REMOVE
    bits += thisBits;
    unsigned char mask = 0xFF >> (8-thisBits);
    codePoint <<= thisBits;
    unsigned char newBits = c & mask;
    codePoint |= newBits;
//    verbose(1, "codepoint=%04x c=%02x mask=%02x newBits=%02x \n", codePoint, c, mask, newBits);  // DEBUG REMOVE 

    if (remaining == 0)
	break;

    c = *s++;
//    verbose(1, "c=%c=%02x\n", c, (unsigned)c);  // DEBUG REMOVE
    if ((c & 0xC0) != 0x80)
	errAbort("missing expected leading bits 10 in char %c", c);
    thisBits = 6;  // remaining utf8 multi-bytes should always have 6 bits.
    --remaining;
    }
if (retBits)
    *retBits = bits;
if (retBytes)
    *retBytes = bytes;
return codePoint;
}

unsigned int utf8Codepoint(char *s)
/* Return the unicode codepoint for a valid utf8 multi-byte character */
{
return utf8CodepointExt(s, NULL, NULL);
}

void extraValidateUtf8Codepoint(char *s, int *retBits, int *retBytes)
/* Validate a utf8 character further by checking for TOO-WIDE chars
 * and other issues. */
{
int bits = 0;
int bytes = 0;
unsigned int codePoint = utf8CodepointExt(s, &bits, &bytes);
if ( 
((codePoint < 0x80   ) && (bytes > 1)) ||
((codePoint < 0x800  ) && (bytes > 2)) || 
((codePoint < 0x10000) && (bytes > 3)) 
)
    errAbort("invalid codePoint too wide.");
}

boolean isValidUtf8(char *s)
/* Is the input string a valid utf8 string */
{
char c = 0;
boolean inUtf8 = FALSE;
int remaining = 0;
while ((c = *s++))
    {
    if (inUtf8)
	{
	if ((c & 0xC0) != 0x80)
	    return FALSE;
	if (remaining == 0)
	    return FALSE;
	--remaining;
	if (remaining == 0)
	    inUtf8 = FALSE;
	}
    else
	{
	if (c < 0)
	    {
	    remaining = -1;
	    while ((c & 0x80) == 0x80)
		{
		++remaining;
		c <<= 1;
		}
	    if (remaining < 1)  // remainder of utf8 should have at least one more byte
		return FALSE;
	    if (remaining > 3)  // remainder of utf8 should have no more than three more bytes
		return FALSE;
	    inUtf8 = TRUE;
	    }
	}
    }
if (inUtf8)  // at the end inUtf8 should be FALSE
    return FALSE;

return TRUE;

}

char *translateUtf8ToWindows1252(char *utf8String)
/* Translate a valid utf8 string into a windows-1252 encoded string. */
{
int l = strlen(utf8String);
// it can only shrink in translation.
// unless using HTML Entities
if (useHtmlEntities)
    l = l * 10;  // worst case code-expansion
char *w = needMem(l+1);  
char *s = w;
while (*utf8String != 0)
    {
    int c = countUtf8WidthInBytes(utf8String);
//    verbose(1, "translateTo1252: count of utf8 width in bytes: %d\n", c);  // DEBUG REMOVE
    if (c == 1) // regular char
	{
	*s++ = *utf8String; 
	}
    else  // utf8 multi-byte char, translate with hash from table.
	{
	// add zero-termination to string?
	char saveChar = utf8String[c];
	utf8String[c] = 0;
	struct hashEl *hel = hashLookup(hash, utf8String);
	if (hel)
	    {
	    *s++ = ptToChar(hel->val);
	    verbose(1, "FYI translated with hash\n");  // DEBUG REMOVE
	    }
	else
	    {  // doing nothing drops the unmapped character.
	    unsigned int codePoint = utf8Codepoint(utf8String);
	    verbose(1, "Unknown utf8 multi-byte char: %s codePoint=%u\n", 
		utf8String, codePoint);
	    ++unmappedCharCount;
	    if (useHtmlEntities)
		{
		// Can replace it with an html-entity for the codepoint like the browsers do.
		char codeString[20];
		safef(codeString, sizeof codeString, "&#%d;", codePoint);
		strcpy(s, codeString);
		s += strlen(codeString);
		}	
	    }	
	utf8String[c] = saveChar;  // restore
	} 
    utf8String += c;
    }
*s = 0;  // terminate final string.
return w;
}

void fixValue(char *profile, char *database, char *table, char *field, char *fieldVal, char *fixedVal)
/* Repair a value in profile database.table.field with fixed value fixedVal. */
{
struct sqlConnection *conn = sqlConnectProfile(profile, database);
char query[512];

sqlSafef(query, sizeof(query), "update %s set %s = '%s' where %s = '%s'", table, field, fixedVal, field, fieldVal);
int numChanged = sqlUpdateRows(conn, query, NULL);
sqlDisconnect(&conn);
if (numChanged != 1)
    verbose(1, "fixing value %s to %s, rows changed = %d\n", fieldVal, fixedVal, numChanged);
}


void galtFixUtf8Chars(char *profile, char *database, char *table, char *field)
/* Find files that have not been symlinked to cdw/ yet to save space. */
{
verbose(1, "profile=%s %s.%s.%s\n", profile, database, table, field);
struct sqlConnection *conn = sqlConnectProfile(profile, database);
char query[512];
struct sqlResult *sr;
char **row;

sqlSafef(query, sizeof(query), "select %s from %s cdwSubmit", field, table);
sr = sqlGetResult(conn, query);
int rowCount = 0;
int fixCount = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *fieldVal = row[0];
    ++rowCount;

    if (cgiDecodeValues)  // remove extra layer of %HH encoding
	{
	fieldVal = cloneString(row[0]);
	cgiDecodeFull(fieldVal, fieldVal, strlen(fieldVal));
	}

    if (!hasAsciiCharsOnly(fieldVal))
	{
	// reporting the names here means we do not see plain ascii names which is good.
	verbose(1, "-----------------------------------------\n");  
	verbose(1, "%s\n", fieldVal);
	verbose(1, "%s has non-ASCII characters\n", fieldVal);
	// NOTE this test is not perfect (because of false positives), but usually works:
	if (isValidUtf8(fieldVal))  
	    {
	    verbose(1, "%s has valid UTF8 characters\n", fieldVal);
	    char *fixedVal = translateUtf8ToWindows1252(fieldVal);
	    
	    if (fix)
		{
		verbose(1, "fix not implemented yet. fixedVal=%s\n", fixedVal);
		fixValue(profile, database, table, field, fieldVal, fixedVal);
		}
	    else
		{
		verbose(1, "fix command not specified, skipping fix. fixedVal=%s\n", fixedVal);
		}
	    // multiple rows with the same field value before or after translation? 
	    struct hashEl *hel = hashLookup(dupeVals, fixedVal);
	    if (hel)
		{
		verbose(1, "FYI duplicate fixed field value found: %s\n", fixedVal);
		++dupeCount;
		}
	    else
		{
		hashAdd(dupeVals, fieldVal, NULL);
		}

	    ++fixCount;
	    }
	else
	    {
	    verbose(1, "%s Does NOT have valid UTF8 characters\n", fieldVal);
	    }

	}

    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);

verbose(1, "\n\n%d utf-8 field values %sconverted to windows-1252 (a superset of iso-8859-1).\n", fixCount, fix ? "":"would have been ");

verbose(1, "%d rows in %s.%s.%s\n", rowCount, database, table, field);

verbose(1, "%d times a utf-8 multi-byte character was missing from the translation hash.\n", unmappedCharCount);

verbose(1, "%d times a duplicate fixed value was found.\n", dupeCount);

if (!fix)
    verbose(1, "\nfix command was not specified. Dry run only.\n\n");

}


void loadUtf8Hash()
/* load utf8 hash to translate utf8 to windows-1252 characters */
{
hash = newHash(10);
struct lineFile *lf = lineFileOpen("utf8.text", TRUE);  // DEBUG RESTORE
// incoming lines are in this form:
//XX utf8
// where XX is the hex code for the windows-1252 
// (a popular superset of iso-8859-1)
char *line;
while(lineFileNext(lf, &line, NULL))
    {
    //printf("%s\n", line);
    line[2] = 0;
    char *utf8 = line+3;
    if (utf8[0] != 0)
	{
	// convert hex chars representing a windows-1252 char into a char.
	unsigned char c = hexToByte(line);  
	// add hash key of utf8 multi-char string.
	hashAdd(hash, utf8,( void *)charToPt(c));
	}
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();

useHtmlEntities = optionExists("useHtmlEntities");
cgiDecodeValues = optionExists("cgiDecode");

char *command = argv[1];
verbose(1, "command=%s\n", command);

if (sameString(command, "fix"))
    fix = TRUE;
else if (sameString(command, "find"))
    fix = FALSE;
else
   usage(); 

loadUtf8Hash();  // for translation

dupeVals = newHash(10);  // for duplicate value detection

galtFixUtf8Chars(argv[2], argv[3], argv[4], argv[5]);  // DEBUG RESTORE

/*
char *fieldVal = "cat\x90\xA0\xB0\xC0\xD0\xE0";
if (hasAsciiCharsOnly(fieldVal))
    verbose(1, "%s has only ASCII characters\n", fieldVal);
else
    verbose(1, "%s has non-ASCII characters\n", fieldVal);
*/

/*
char *fieldVal = "cat\xFF\x80";
if (isValidUtf8(fieldVal))
    verbose(1, "%s has valid UTF8 characters\n", fieldVal);
else
    verbose(1, "%s Does NOT have valid UTF8 characters\n", fieldVal);
*/

/*
// generate iso-8859-1 text table:
int i;
for(i=128;i<256;++i)
    {
    printf("%2x ", i);
    putc(i, stdout);
    printf("\n");
    }
*/

return 0;
}
