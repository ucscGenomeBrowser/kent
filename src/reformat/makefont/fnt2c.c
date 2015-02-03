
/* cfont.c  - Stuff to load and free up the 'user' (as opposed to built 
	in system) font */
#include "common.h"
#include "gemfont.h"

/* make a disk copy (in C) of font in memory. */
print_font(name, font, cf_offsets, cf_offset_size, cf_data, cf_data_size)
char *name;
struct font_hdr *font;
WORD *cf_offsets;
long cf_offset_size;
UBYTE *cf_data;
long cf_data_size;
{
FILE *f;
long i;
char sbuf[256];
int j, offsets;

sprintf(sbuf, "%s.c", name);
if ((f = fopen(sbuf, "w")) == NULL)
	{
	fprintf(stderr, "Couldn't open %s\n", sbuf);
	return(0);
	}
fprintf(f, "UBYTE %s_data[] = {\n", name);
for (i=0; i<cf_data_size; i++)
	{
	fprintf(f, "0x%x,", cf_data[i]);
	if ((i&7)==7)
		fprintf(f, "\n");
	}
fprintf(f, "};\n\n");
uglyf("printed data ok\n");
if (cf_offsets != NULL)
    {
    offsets = cf_offset_size/sizeof(WORD);
    fprintf(f, "WORD %s_ch_ofst[%d] = {\n", name, offsets);
    uglyf("printed offset name ok cf_offsets %p offsets %d\n", cf_offsets, offsets);
    for (j=0; j<offsets; j++)
	    {
	    fprintf(f, "%d,", cf_offsets[j]);
	    if ((j&7)==7)
		    fprintf(f, "\n");
	    }
    uglyf("printed offsets ok\n");
    fprintf(f, "};\n\n");
    }
fprintf(f, "struct font_hdr %s_font = {\n", name);
fprintf(f, "0x%x, %d, \"%s\", %d, %d,\n",
	font->id, font->size, name, font->ADE_lo, font->ADE_hi);
fprintf(f, "%d, %d, %d, %d, %d,\n",
	font->top_dist, font->asc_dist, font->hlf_dist, font->des_dist,
	font->bot_dist);
fprintf(f, "%d, %d, %d, %d,\n",
	font->wchr_wdt, font->wcel_wdt, font->lft_ofst, font->rgt_ofst);
fprintf(f, "%d, %d, 0x%x, 0x%x,\n",
	font->thckning, font->undrline, font->lghtng_m, font->skewng_m);
if (cf_offsets != NULL)
    fprintf(f, "0x%x, NULL, %s_ch_ofst, %s_data,\n",
	    font->flags, name, name);
else
    fprintf(f, "0x%x, NULL, NULL, %s_data,\n",
	    font->flags, name);

fprintf(f, "%d, %d,\n", font->frm_wdt, font->frm_hgt);
fprintf(f, "NULL,};\n");
fclose(f);
return(1);
}

void intel_swap(char *bytes, int word_count)
{
int i;
for (i=0; i<word_count; ++i)
    {
    char temp = bytes[0];
    bytes[0] = bytes[1];
    bytes[1] = temp;
    bytes += 2;
    }
}

/* Load up a font chosen by user. */
boolean load_cfont(char *title, struct  font_hdr *font)
{
FILE *f;
long cf_offset_size, cf_data_size;
UBYTE *cf_data;
WORD *cf_offsets;
long lid9 = 0x9000;
long lidB = 0xB000;
WORD id9 = lid9;
WORD idB = lidB;
if ((f = fopen(title, "rb")) == NULL)
    {
    fprintf(stderr, "Can't find %s\n", title);
    return FALSE;
    }
if (fread(font, sizeof(*font), 1, f) < 1)
    {
    fprintf(stderr, "Can't read header for %s\n", title);
    return FALSE;
    }
intel_swap((char *)(&font->ADE_lo),16);
intel_swap((char *)(&font->frm_wdt),2);
cf_offset_size = font->ADE_hi - font->ADE_lo + 2;
if (cf_offset_size > 300 || cf_offset_size <= 2)
    {
    fprintf(stderr, "Bad font file format in %s\n", title);
    return FALSE;
    }
cf_offset_size *= sizeof(WORD);
if ((cf_offsets = malloc(cf_offset_size)) == NULL)
    {
    fprintf(stderr, "Out of memory in %s\n", title);
    return FALSE;
    }
if (fread(cf_offsets, 1, cf_offset_size, f) < cf_offset_size )
    {
    fprintf(stderr, "%s - file truncated\n", title);
    return FALSE;
    }
cf_data_size = font->frm_wdt;
cf_data_size *= font->frm_hgt;
if ((cf_data = malloc(cf_data_size)) == NULL)
    {
    fprintf(stderr, "Out of memory in %s\n", title);
    return FALSE;
    }
if (fread(cf_data, 1, cf_data_size, f) < cf_data_size )
    {
    fprintf(stderr, "%s - file truncated\n", title);
    return FALSE;
    }
intel_swap((char *)cf_offsets, cf_offset_size/sizeof(WORD));
font->ch_ofst =  (WORD *)cf_offsets;
font->fnt_dta = cf_data;
if (font->flags & 4)	/* swapped... */
    {
    intel_swap(cf_data, cf_data_size/sizeof(WORD));
    }
font->hz_ofst = NULL;
if ( (font->id)==(id9))
    {
    font->id = MPROP;
    if ((font->hz_ofst = malloc(cf_offset_size)) == NULL)
	    {
	    fprintf(stderr, "%s - out of memory\n", title);
	    return FALSE;
	    }
    if (fread(font->hz_ofst, 1, cf_offset_size, f) < cf_offset_size )
	    {
	    fprintf(stderr, "%s - file truncated\n", title);
	    free(font->hz_ofst);
	    return FALSE;
	    }
    }
else if ((font->id) == idB)
    font->id = MFIXED;
else
    font->id = STPROP;
fclose(f);
return(TRUE);
}


main(int argc, char *argv[])
{
if (argc != 3)
    {
    printf("Usage: makefont input.fnt output\n");
    exit(0);
    }
printf("Creating %s.c\n", argv[2]);
    {
    struct font_hdr font;
    if (load_cfont(argv[1], &font))
	{
	print_font(argv[2], &font,
	    font.ch_ofst, (font.ADE_hi - font.ADE_lo + 2) * sizeof(WORD),
	    font.fnt_dta, font.frm_wdt * font.frm_hgt);
	}
    }
}
