/* freen - My Pet Freen. */
#include "common.h"
#include <sys/types.h>
#include "errabort.h"
#include "sqlNum.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "freen - My pet freen\n"
  "usage:\n"
  "   freen numBytes fileName\n");
}

/* stuff for my buffered io */
#define BSIZE (64*1024)
struct bFile
	{
	int fd;
	int left;
	UBYTE *buf;
	UBYTE *filept;
	int writable;
	};

void bFlush(struct bFile *bf)
/* Do any pending writes to buffered file.  Force next read to be
   straight from disk */
{
unsigned long size, wsize;

if (bf->writable)
    {
    size = BSIZE-bf->left;
    if (size > 0)
	{
	wsize = write(bf->fd, bf->buf, size);
	bf->filept = bf->buf;
	bf->left = BSIZE;
	if (wsize < size)
            errnoAbort("Write error");
	}
    }
else
    {
    bf->left = 0;
    }
}



void bClose(struct bFile **pBf)
/* flush and close a buffered file.  Free up memory used by buffer. */
{
int ok = TRUE;
struct bFile *bf;

if ((bf = *pBf) != NULL)
    {
    if (bf->writable)
	{
	bFlush(bf);
	}
    if (bf->fd != 0)
	(bf->fd);
    freeMem(bf->buf);
    freez(pBf);
    }
}

static struct bFile *bFileNew()
/* Get buffer for a bFile. */
{
struct bFile *bf;
AllocVar(bf);
bf->buf = needMem(BSIZE);
return bf;
}

struct bFile *bOpen(char *name)
/* Open up a buffered file to read */
{
struct bFile *bf = bFileNew();
if ((bf->fd = open(name, O_RDONLY)) < 0)
    errnoAbort("Couldn't open %s", name);
return bf;
}

struct bFile *bCreate(char *name)
/* Create a buffered file to write */
{
struct bFile *bf = bFileNew();
if ((bf->fd = creat(name, 0666)) < 0)
    {
    errnoAbort("Couldn't create %s", name);
    }
bf->writable = 1;
bf->left = BSIZE;
bf->filept = bf->buf;
return bf;
}

off_t bSeek(struct bFile *bf, off_t offset, int mode)
/* Seek to character position.  Mode parameter same as MS-DOS.  IE 
	mode = 0  for seek from beginning */
{
bFlush(bf);
return lseek(bf->fd,offset,mode);
}

void bPutByte(struct bFile *bf, char c)
/* Write out a single byte to buffered file */
{
*bf->filept++ = c;
if (--bf->left <= 0)
    {
    bFlush(bf);
    }
}

int bWrite(struct bFile *bf, char *buf, size_t count)
/* Write out a block (32K or less) to buffered file */
{
int i;
if (count <= 0)
    return(0);
for (i=0; i<count; i++)
    {
    *bf->filept++ = *buf++;
    if (--bf->left <= 0)
	{
	bFlush(bf);
	}
    }
return(count);
}

int bGetByte(struct bFile *bf)
/* Read next byte of buffered file */
{
if (--bf->left < 0)
    {
    bf->left = read(bf->fd, bf->buf, BSIZE);
    if (bf->left <= 0)
        {
        return EOF;
	}
    --(bf->left);
    bf->filept = bf->buf;
    }
return(*bf->filept++);
}

int bRead(struct bFile *bf, char *buf, int count)
/* Read from buffered file */
{
int c,i;

if (count <= 0)
    return(0);
for (i=0; i<count; i++)
    {
    if ((c = bGetByte(bf)) < 0)
	return(i);
    *buf++ = c;
    }
return(count);
}



void biggaBigga(unsigned numBytes, char *fileName)
/* Make a great big file. */
{
UBYTE c = 'a';
unsigned i;
struct bFile *bf;

printf("Making big file %s of %u bytes\n", fileName, numBytes);
bf = bCreate(fileName);
for (i=0; i<numBytes; ++i)
    {
    bPutByte(bf, c);
    if (c == '\n')
        c = 'a';
    else
        {
	c += 1;
	if (c > 'z')
	    c = '\n';
	}
    if ((i&0x3ff) == 0)
        {
	if ((i&0x3fffff) == 0)
	    printf("Byte %u\n", i);
	}
    }
bClose(&bf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3 || !isdigit(argv[1][0]))
    usage();
biggaBigga(sqlUnsigned(argv[1]), argv[2]);
return 0;
}
