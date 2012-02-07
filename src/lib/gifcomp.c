/* comprs.c - LZW compression code for GIF */

/*
 * ABSTRACT:
 *	The compression algorithm builds a string translation table that maps
 *	substrings from the input string into fixed-length codes.  These codes
 *	are used by the expansion algorithm to rebuild the compressor's table
 *	and reconstruct the original data stream.  In it's simplest form, the
 *	algorithm can be stated as:
 *
 *		"if <w>k is in the table, then <w> is in the table"
 *
 *	<w> is a code which represents a string in the table.  When a new
 *	character k is read in, the table is searched for <w>k.  If this
 *	combination is found, <w> is set to the code for that combination
 *	and the next character is read in.  Otherwise, this combination is
 *	added to the table, the code <w> is written to the output stream and
 *	<w> is set to k.
 *
 *	The expansion algorithm builds an identical table by parsing each
 *	received code into a prefix string and suffix character.  The suffix
 *	character is pushed onto the stack and the prefix string translated
 *	again until it is a single character.  This completes the expansion.
 *	The expanded code is then output by popping the stack and a new entry
 *	is made in the table.
 *
 *	The algorithm used here has one additional feature.  The output codes
 *	are variable length.  They start at a specified number of bits.  Once
 *	the number of codes exceeds the current code size, the number of bits
 *	in the code is incremented.  When the table is completely full, a
 *	clear code is transmitted for the expander and the table is reset.
 *	This program uses a maximum code size of 12 bits for a total of 4096
 *	codes.
 *
 *	The expander realizes that the code size is changing when it's table
 *	size reaches the maximum for the current code size.  At this point,
 *	the code size in increased.  Remember that the expander's table is
 *	identical to the compressor's table at any point in the original data
 *	stream.
 *
 *	The compressed data stream is structured as follows:
 *		first byte denoting the minimum code size
 *		one or more counted byte strings. The first byte contains the
 *		length of the string. A null string denotes "end of data"
 *
 *	This format permits a compressed data stream to be embedded within a
 *	non-compressed context.
 *
 * AUTHOR: Steve Wilhite
 *
 * REVISION HISTORY:
 *   Speed tweaked a bit by Jim Kent 8/29/88
 *
 */

#include "common.h"
#include <setjmp.h>


#define UBYTE unsigned char


#define LARGEST_CODE	4095
#define TABLE_SIZE	(8*1024)

static UBYTE gif_byte_buff[256+3];               /* Current block */
static FILE *gif_file;

static unsigned char *gif_wpt;
static long gif_wcount;

static jmp_buf recover;

static short *prior_codes;
static short *code_ids;
static unsigned char *added_chars;

static short code_size;
static short clear_code;
static short eof_code;
static short bit_offset;
static short max_code;
static short free_code;


static void init_table(short min_code_size)
{
code_size = min_code_size + 1;
clear_code = 1 << min_code_size;
eof_code = clear_code + 1;
free_code = clear_code + 2;
max_code = 1 << code_size;

zeroBytes(code_ids, TABLE_SIZE*sizeof(code_ids[0]));
}


static void flush(size_t n)
{
if (fputc(n,gif_file) < 0)
    {
    longjmp(recover, -3);
    }
if (fwrite(gif_byte_buff, 1, n, gif_file) < n)
    {
    longjmp(recover, -3);
    }
}


static void write_code(short code)
{
long temp;
register short byte_offset; 
register short bits_left;

byte_offset = bit_offset >> 3;
bits_left = bit_offset & 7;

if (byte_offset >= 254)
	{
	flush(byte_offset);
	gif_byte_buff[0] = gif_byte_buff[byte_offset];
	bit_offset = bits_left;
	byte_offset = 0;
	}

if (bits_left > 0)
	{
	temp = ((long) code << bits_left) | gif_byte_buff[byte_offset];
	gif_byte_buff[byte_offset] = (UBYTE)temp;
	gif_byte_buff[byte_offset + 1] = (UBYTE)(temp >> 8);
	gif_byte_buff[byte_offset + 2] = (UBYTE)(temp >> 16);
	}
else
	{
	gif_byte_buff[byte_offset] = (UBYTE)code;
	gif_byte_buff[byte_offset + 1] = (UBYTE)(code >> 8);
	}
bit_offset += code_size;
}


/*
 * Function:
 *	Compress a stream of data bytes using the LZW algorithm.
 *
 * Inputs:
 *	min_code_size
 *		the field size of an input value.  Should be in the range from
 *		1 to 9.
 *
 * Returns:
 *	 0	normal completion
 *	-1	(not used)
 *	-2	insufficient dynamic memory
 *	-3	bad "min_code_size"
 *	< -3	error status from either the get_byte or put_byte routine
 */
static short compress_data(int min_code_size)
{
short status;
short prefix_code;
short d;
register int hx;
register short suffix_char;

status = setjmp(recover);

if (status != 0)
    {
    return status;
    }

bit_offset = 0;
init_table(min_code_size);
write_code(clear_code);
suffix_char = *gif_wpt++;
gif_wcount -= 1;

prefix_code = suffix_char;

while (--gif_wcount >= 0)
    {
    suffix_char = *gif_wpt++;
    hx = prefix_code ^ suffix_char << 5;
    d = 1;

    for (;;)
	{
	if (code_ids[hx] == 0)
	    {
	    write_code(prefix_code);

	    d = free_code;

	    if (free_code <= LARGEST_CODE)
		{
		prior_codes[hx] = prefix_code;
		added_chars[hx] = (UBYTE)suffix_char;
		code_ids[hx] = free_code;
		free_code++;
		}

	    if (d == max_code)
		{
		if (code_size < 12)
		    {
		    code_size++;
		    max_code <<= 1;
		    }
		else
		    {
		    write_code(clear_code);
		    init_table(min_code_size);
		    }
	        }

	    prefix_code = suffix_char;
	    break;
	    }

	if (prior_codes[hx] == prefix_code &&
		added_chars[hx] == suffix_char)
	    {
	    prefix_code = code_ids[hx];
	    break;
	    }

	hx += d;
	d += 2;
	if (hx >= TABLE_SIZE)
	    hx -= TABLE_SIZE;
	}
    }

write_code(prefix_code);

write_code(eof_code);


/* Make sure the code buffer is flushed */

if (bit_offset > 0)
    {
    int byte_offset = (bit_offset >> 3);
    if (byte_offset == 255)	/* Make sure we don't write a zero by mistake. */
        {
	int bits_left = bit_offset & 7;
	flush(255);
	if (bits_left)
	    {
	    gif_byte_buff[0] = gif_byte_buff[byte_offset];
	    flush(1);
	    }
	}
    else
	{
	flush((bit_offset + 7)/8);
	}
    }

flush(0);				/* end-of-data */
return 0;
}

short gif_compress_data(int min_code_size, unsigned char *pt, long size, FILE *out)
{
int ret;

/* Make sure min_code_size is reasonable. */
if (min_code_size < 2 || min_code_size > 9)
    {
    if (min_code_size == 1)
	min_code_size = 2;
    else
	return -3;
    }

/* Store input parameters where rest of routines can use. */
gif_file = out;
gif_wpt = pt;
gif_wcount = size;

ret = -2;	/* out of memory default */
prior_codes = NULL;
code_ids = NULL;
added_chars = NULL;
if ((prior_codes = (short*)needMem(TABLE_SIZE*sizeof(short))) == NULL)
	goto OUT;
if ((code_ids = (short*)needMem(TABLE_SIZE*sizeof(short))) == NULL)
	goto OUT;
if ((added_chars = (unsigned char*)needMem(TABLE_SIZE)) == NULL)
	goto OUT;

ret = compress_data(min_code_size);

OUT:
gentleFree(prior_codes);
gentleFree(code_ids);
gentleFree(added_chars);
return(ret);
}

