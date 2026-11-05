/* cgiDecodeTest - check cgiDecode on well-formed and malformed %hh escapes, and that it
 * never reads past the input it is given.
 *
 * cgiDecode reads the two hex digits of an escape itself instead of with sscanf.  sscanf
 * scanned to the end of the string on every call, so decoding took time quadratic in the
 * length of the value, and a cart variable can be megabytes long.  This test prints what each
 * input decodes to, including escapes cut short at the end of the input and escapes whose
 * digits are not hex, and then checks that decoding never looks past the input's length,
 * which is the read-ahead that made the old code slow.  refs #37262 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "cheapcgi.h"
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

static char *cases[] = {
"plain",
"a+b+c",
"%41%42%43",
"%4a%4A%4b",        /* lower and upper case hex digits */
"100%25",
"%2B+%20",           /* an escaped plus is a plus, an unescaped one is a space */
"%00after",          /* an escaped null ends the decoded string */
"%zzx",              /* neither digit is hex */
"%4gx",              /* only the first digit is hex */
"%g4x",              /* only the second digit is hex */
"end%",              /* escape cut short at the end of the input */
"end%4",
"%",
"%%41",
"%1F%7f%FF",
"",
};

static void printBytes(char *s)
/* Print s with any byte outside printable ASCII shown as \xhh. */
{
unsigned char *u;
for (u = (unsigned char *)s; *u != 0; u++)
    {
    if (*u >= ' ' && *u < 0x7f && *u != '\\')
        putchar(*u);
    else
        printf("\\x%02x", *u);
    }
}

static void showCase(char *in)
/* Decode a copy of in and print the result.  Decode in place, since callers do. */
{
char *buf = cloneString(in);
cgiDecode(buf, buf, strlen(buf));
printf("in  : %s\nout : ", in);
printBytes(buf);
printf("\n");
freeMem(buf);
}

static void segvHandler(int sig)
/* Say what went wrong before dying, so the diff shows it. */
{
static char msg[] = "cgiDecode read past the end of its input\n";
if (write(STDOUT_FILENO, msg, sizeof(msg) - 1) < 0)
    _exit(2);
_exit(1);
}

static void noReadAheadCheck()
/* Decode a value that ends exactly at the end of a page, followed by a page that can not be
 * read, and with no terminating null.  cgiDecode is given the length, so it has no reason to
 * look past it.  The sscanf version read to the next null on every escape, which is what
 * made it quadratic: 600,000 escapes took about 5 seconds.  Here that read-ahead lands on the
 * unreadable page and the test dies, so it counts the work instead of timing it. */
{
static char value[] = "%41+%42%2b";
int len = strlen(value);
long pageSize = sysconf(_SC_PAGESIZE);
char *pages = mmap(NULL, 2 * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);
if (pages == MAP_FAILED)
    errnoAbort("mmap");
if (mprotect(pages + pageSize, pageSize, PROT_NONE) != 0)
    errnoAbort("mprotect");
char *in = pages + pageSize - len;
memcpy(in, value, len);
char out[64];
fflush(stdout);
signal(SIGSEGV, segvHandler);
cgiDecode(in, out, len);
signal(SIGSEGV, SIG_DFL);
printf("no read-ahead: %s decodes to \"%s\" without reading past its length\n", value, out);
munmap(pages, 2 * pageSize);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
for (i = 0; i < ArraySize(cases); i++)
    showCase(cases[i]);
noReadAheadCheck();
return 0;
}
