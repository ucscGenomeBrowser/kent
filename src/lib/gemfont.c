/* gemfont.c - Raster Font stuff, draws text based on a blit and a font
   in a format that some day may approach Ventura Publisher, but currently
   looks much more like GEM on the ST with some Mac-like mutations. */

#include "common.h"
#include "memgfx.h"
#include "gemfont.h"


typedef union
    {
    int  theInt;
    char bytes[2];
    } myInt;

void gfText(struct memGfx *screen, struct font_hdr *f, char *text, 
       int x, int y, Color color, TextBlit tblit, Color bcolor)
{
UBYTE *s = (UBYTE*)text;
UBYTE *ss;
int c, lo, hi;
int sx, imageWid;
WORD *off, wd, ht;
UBYTE *data;
myInt *OWtab, *iPtr;
int missChar;
int font_type;
int extraWidth = f->lft_ofst + f->rgt_ofst;

x += f->xOff;
y += f->yOff;
x += f->lft_ofst;
lo = f->ADE_lo;
hi = f->ADE_hi;
off = f->ch_ofst;
wd = f->frm_wdt;
ht = f->frm_hgt,
data = f->fnt_dta;
OWtab= (myInt *)(f->hz_ofst);
font_type = f->id;

while ((c = *s++)!=0)
    {
    /* If we don't have the character, just turn it into a space. */
    if (c > hi)
	{
	c = ' ';
	}
    c -= lo;
    if (c < 0)
	{
	c = ' ' - lo;
	}

    /* Mac prop font && its a missing char */
    if (font_type == MPROP && (*(OWtab+c)).theInt == -1) 
	{            
	c=hi-lo;                      /* last char is set */
	missChar=1;
	sx = off[c+1];
	imageWid= f->frm_wdt*8 - sx;  /* sort of a kludge */
	}
    else 
	{
	missChar=0;
	sx = off[c];
	imageWid = off[c+1]-sx;
	}
    (*tblit)(imageWid, ht, sx, 0, data, wd, screen, x, y, color, bcolor);
    switch (font_type)
	{
	case STPROP:
	    x += imageWid + extraWidth;
	    break;
	case MFIXED:
	    x += f->wchr_wdt + extraWidth;          
	    break;
	case MPROP:
	    iPtr=OWtab+c;  
	    if (!missChar)
		    /* -1 means its a missing character */
		{
		x += (int)((*iPtr).bytes[1]);
		ss=s;
		if ((c=*(ss++)) != 0)
			/* look to next char to determine amt to change x */
		    {
		    c-= lo;
		    iPtr=OWtab+c;
		    /* subtract kern Of Next char */
		    /* f->rgt_ofst is neg of Mac maxKern value */
		    if ((*iPtr).theInt!=-1)
		       x += (int)((*iPtr).bytes[0])+ f->rgt_ofst;  
		    }           
		}
	    else /* display the non print char */
		x+=imageWid + extraWidth;
	    break;
	}
    }
}

static int fchar_width(struct font_hdr *f,unsigned char *s)
/* How wide is this character? */
{
int c;
signed char *offsets;
int width;
int t;

c = *s++;
if (c > f->ADE_hi)
    c = ' ';
c -= f->ADE_lo;
if (c < 0)
    {
    c = ' ' - f->ADE_lo;
    }
switch (f->id)
    {
    case MFIXED:
	    return(f->wchr_wdt + f->lft_ofst + f->rgt_ofst);
    case STPROP:
	    return(f->ch_ofst[c+1] - f->ch_ofst[c] + f->lft_ofst + f->rgt_ofst);
    case MPROP:
	    offsets = f->hz_ofst+c*2;
	    if (offsets[0] == -1 && offsets[1] == -1)	/* missing char */
		{
		t = f->ADE_hi - f->ADE_lo;
		return( f->frm_wdt*8 - f->ch_ofst[t+1]);
		}
	    else
		{
		width = offsets[1];
		if ((c = *s++) != 0)
			{
			c -= f->ADE_lo;
			offsets = f->hz_ofst+c*2;
			width += offsets[0] + f->rgt_ofst;
			}
		return(width);
		}
    default:
         internalErr();
	 return 0;
    }
}

long fnstring_width(struct font_hdr *f, unsigned char *s, int n)
{
long acc = 0;

while (--n >= 0)
    {
    acc += fchar_width(f, s);
    s++;
    }
return(acc);
}

#if 0 /* unused */
static long fstring_width(struct font_hdr *f, unsigned char *s)
{
return(fnstring_width(f, s, strlen((char *)s)));
}
#endif


int fwidest_char(struct font_hdr *f)
{
unsigned char buf[2];
int i;
int c;
int widest = 1;
int w;

c = f->ADE_lo;
i = f->ADE_hi - c;
buf[1] = 0;
while (--i >= 0)
	{
	buf[0] = c++;
	w = fchar_width(f, buf);
	if (w > widest)
		widest = w;
	}
return(widest);
}

int font_cel_height(struct font_hdr *f)
/* How tall is font? */
{
return f->frm_hgt;
}

int font_line_height(struct font_hdr *f)
/* How far to next line. */
{
return f->lineHeight;
}
