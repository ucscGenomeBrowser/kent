/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "samAlignment.h"
#include "dnaseq.h"
#include "bamFile.h"
#include "htmshell.h"
#include "rainbow.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}


UBYTE init_cmap[256*3] = {
 /* 32 level grey scale */
 0, 0, 0,	 2, 2, 2,	 4, 4, 4,	 6, 6, 6,	
 8, 8, 8,	10,10,10,	12,12,12,	14,14,14,	
16,16,16,	18,18,18,	20,20,20,	22,22,22,	
24,24,24,	26,26,26,	28,28,28,	30,30,30,	
33,33,33,	35,35,35,	37,37,37,	39,39,39,	
41,41,41,	43,43,43,	45,45,45,	47,47,47,	
49,49,49,	51,51,51,	53,53,53,	55,55,55,	
57,57,57,	59,59,59,	61,61,61,	63,63,63,	
/* A hand threaded 6x6x6 rgb cube */
63,51,51,	63,63,51,	51,63,51,	51,63,63,	
51,51,63,	63,51,63,	63,39,39,	63,51,39,	
63,63,39,	51,63,39,	39,63,39,	39,63,51,	
39,63,63,	39,51,63,	39,39,63,	51,39,63,	
63,39,63,	63,39,51,	63,27,27,	63,39,27,	
63,51,27,	63,63,27,	51,63,27,	39,63,27,	
27,63,27,	27,63,39,	27,63,51,	27,63,63,	
27,51,63,	27,39,63,	27,27,63,	39,27,63,	
51,27,63,	63,27,63,	63,27,51,	63,27,39,	
63,15,15,	63,27,15,	63,39,15,	63,51,15,	
63,63,15,	51,63,15,	39,63,15,	27,63,15,	
15,63,15,	15,63,27,	15,63,39,	15,63,51,	
15,63,63,	15,51,63,	15,39,63,	15,27,63,	
15,15,63,	27,15,63,	39,15,63,	51,15,63,	
63,15,63,	63,15,51,	63,15,39,	63,15,27,	
63, 3,15,	63, 3, 3,	63,15, 3,	63,27, 3,	
63,39, 3,	63,51, 3,	63,63, 3,	51,63, 3,	
39,63, 3,	27,63, 3,	15,63, 3,	 3,63, 3,	
 3,63,15,	 3,63,27,	 3,63,39,	 3,63,51,	
 3,63,63,	 3,51,63,	 3,39,63,	 3,27,63,	
 3,15,63,	 3, 3,63,	15, 3,63,	27, 3,63,	
39, 3,63,	51, 3,63,	63, 3,63,	63, 3,51,	
63, 3,39,	63, 3,27,	51, 3,15,	51, 3, 3,	
51,15, 3,	51,27, 3,	51,39, 3,	51,51, 3,	
39,51, 3,	27,51, 3,	15,51, 3,	 3,51, 3,	
 3,51,15,	 3,51,27,	 3,51,39,	 3,51,51,	
 3,39,51,	 3,27,51,	 3,15,51,	 3, 3,51,	
15, 3,51,	27, 3,51,	39, 3,51,	51, 3,51,	
51, 3,39,	51, 3,27,	39, 3,15,	39, 3, 3,	
39,15, 3,	39,27, 3,	39,39, 3,	27,39, 3,	
15,39, 3,	 3,39, 3,	 3,39,15,	 3,39,27,	
 3,39,39,	 3,27,39,	 3,15,39,	 3, 3,39,	
15, 3,39,	27, 3,39,	39, 3,39,	39, 3,27,	
27, 3,15,	27, 3, 3,	27,15, 3,	27,27, 3,	
15,27, 3,	 3,27, 3,	 3,27,15,	 3,27,27,	
 3,15,27,	 3, 3,27,	15, 3,27,	27, 3,27,	
15, 3, 3,	15,15, 3,	 3,15, 3,	 3,15,15,	
 3, 3,15,	15, 3,15,	27,15,15,	27,27,15,	
15,27,15,	15,27,27,	15,15,27,	27,15,27,	
39,15,15,	39,27,15,	39,39,15,	27,39,15,	
15,39,15,	15,39,27,	15,39,39,	15,27,39,	
15,15,39,	27,15,39,	39,15,39,	39,15,27,	
51,15,15,	51,27,15,	51,39,15,	51,51,15,	
39,51,15,	27,51,15,	15,51,15,	15,51,27,	
15,51,39,	15,51,51,	15,39,51,	15,27,51,	
15,15,51,	27,15,51,	39,15,51,	51,15,51,	
51,15,39,	51,15,27,	51,27,27,	51,39,27,	
51,51,27,	39,51,27,	27,51,27,	27,51,39,	
27,51,51,	27,39,51,	27,27,51,	39,27,51,	
51,27,51,	51,27,39,	51,39,39,	51,51,39,	
39,51,39,	39,51,51,	39,39,51,	51,39,51,	
39,27,27,	39,39,27,	27,39,27,	27,39,39,	
27,27,39,	39,27,39,	 3, 3, 3,	15,15,15,	
27,27,27,	39,39,39,	51,51,51,	63,63,63,	
 /* 3 empty slots */
63,22, 3,	39, 7, 5,	36,36,63,	 
 /* Colors the system would like to use for interface */
0, 0, 0,	 22,22,22,	38,38,38,	52,52,52,	63, 0, 0,	
};

int brightness(int r, int g, int b)
/* Return brightness of color */
{
return r*2 + g*3 + b*2;
}

void freen(char *output)
/* Test some hair-brained thing. */
{
FILE *f = mustOpen(output, "w");
htmStart(f, "Autodesk Animator Palette");
fprintf(f, "Full Autodesk Animator Palette: grayscale + 6*6*6 cube that is rainbow threaded<BR>\n");
// fprintf(f, "<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0>\n");
fprintf(f, "<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=0>\n");
UBYTE *pt = init_cmap;
int row, col;
int colorIx = 0;
for (row=0; row<8; ++row)
    {
    fprintf(f, "<TR>\n");
    for (col=0; col<32; ++col)
        {
	fprintf(f, "<TD BGCOLOR='#%02X%02X%02X'>%03d</TD>\n",
		pt[0]*4, pt[1]*4, pt[2]*4, colorIx);
	pt += 3;
	++colorIx;
	}
    fprintf(f, "</TR>\n");
    }
fprintf(f, "</TABLE>\n");
fprintf(f, "<BR><BR>\n");

fprintf(f, "Just the central rainbow.<BR>\n");
fprintf(f, "<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=0>\n");
int rainStart = 92, rainEnd = 122;
pt = init_cmap + rainStart * 3;
colorIx = rainStart;
fprintf(f, "<TR>\n");
for (colorIx = rainStart; colorIx < rainEnd; ++colorIx)
    {
    fprintf(f, "<TD BGCOLOR='#%02X%02X%02X'>%03d</TD>\n",
	    pt[0]*4, pt[1]*4, pt[2]*4, colorIx);
    pt += 3;
    }
fprintf(f, "</TR>\n");
fprintf(f, "</TABLE>\n");
fprintf(f, "<BR><BR>\n");


double midBright = brightness(0, 0, 255);
double prettyBright = brightness(165,165,165);
double rainbowStep = 1.0/30.0;
double rainbowPos = 0;

fprintf(f, "More detail on central rainbow and normalized version.<BR>\n");
fprintf(f, "<TABLE BORDER=1 CELLPADDING=2 CELLSPACING=0>\n");
pt = init_cmap + rainStart * 3;
for (colorIx = rainStart; colorIx < rainEnd; ++colorIx)
    {
    /* Show indexes. */
    fprintf(f, "<TR>\n");
    fprintf(f, "<TD>%d</TD> ", colorIx - rainStart);
    fprintf(f, "<TD>%d</TD> ", colorIx);

    /* Show raw colors */
    int rawR = pt[0]*4, rawG = pt[1]*4, rawB=pt[2]*4;
    fprintf(f, "<TD BGCOLOR='#%02X%02X%02X'>&nbsp;&nbsp;&nbsp;&nbsp;</TD>\n",
	    rawR, rawG, rawB);
    fprintf(f, "<TD>%d</TD> ", rawR);
    fprintf(f, "<TD>%d</TD> ", rawG);
    fprintf(f, "<TD>%d</TD> ", rawB);

    /* Show normalized to be about same brightness - not so pretty */
    int oneBright=brightness(rawR, rawG, rawB);
    double scale = midBright/oneBright;
    int r = rawR*scale, g = rawG*scale, b = rawB*scale;
    fprintf(f, "<TD BGCOLOR='#%02X%02X%02X'>&nbsp;&nbsp;&nbsp;&nbsp;</TD>\n",
	    r, g, b);
    fprintf(f, "<TD>%d</TD> ", r);
    fprintf(f, "<TD>%d</TD> ", g);
    fprintf(f, "<TD>%d</TD> ", b);

    /* Show raw colors mixed 50% with white */
    int lightR = (rawR + 255)/2, lightG = (rawG + 255)/2, lightB = (rawB + 255)/2;
    fprintf(f, "<TD BGCOLOR='#%02X%02X%02X'>&nbsp;&nbsp;&nbsp;&nbsp;</TD>\n",
	    lightR, lightG, lightB);
    fprintf(f, "<TD>%d</TD> ", lightR);
    fprintf(f, "<TD>%d</TD> ", lightG);
    fprintf(f, "<TD>%d</TD> ", lightB);

    /* Now normalize brightness */
    oneBright=brightness(lightR, lightG, lightB);
    scale = prettyBright/oneBright;
    r = lightR*scale, g = lightG*scale, b = lightB*scale;
    fprintf(f, "<TD BGCOLOR='#%02X%02X%02X'>&nbsp;&nbsp;&nbsp;&nbsp;</TD>\n",
	    r, g, b);
    fprintf(f, "<TD>%d</TD> ", r);
    fprintf(f, "<TD>%d</TD> ", g);
    fprintf(f, "<TD>%d</TD> ", b);

    struct rgbColor col = lightRainbowAtPos(rainbowPos);
    fprintf(f, "<TD BGCOLOR='#%02X%02X%02X'>&nbsp;&nbsp;&nbsp;&nbsp;</TD>\n",
            col.r, col.g, col.b);
    fprintf(f, "<TD>%f</TD> ", rainbowPos);
    rainbowPos += rainbowStep;

    pt += 3;
    fprintf(f, "</TR>\n");
    }
fprintf(f, "</TABLE>\n");
fprintf(f, "<BR><BR>\n");

/* Print out light spectrum in some detail. */
fprintf(f, "Light rainbow from lightRainbowAtPos.<BR>\n");
fprintf(f, "<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0>\n");
int i;
int stepCount = 200;
double colorStep = 1.0/stepCount;
rainbowPos = 0.0;
fprintf(f, "<TR>\n");
for (i=0; i<stepCount; ++i)
    {
    struct rgbColor col = lightRainbowAtPos(rainbowPos);
    fprintf(f, "<TD BGCOLOR='#%02X%02X%02X'>&nbsp;</TD>\n",
            col.r, col.g, col.b);
    rainbowPos += colorStep;
    }
fprintf(f, "</TR>\n");
fprintf(f, "</TABLE>\n");
fprintf(f, "<BR><BR>\n");

/* Print out saturated spectrum in some detail. */
fprintf(f, "Saturated rainbow from saturatedRainbowAtPos.<BR>\n");
fprintf(f, "<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0>\n");
rainbowPos = 0.0;
fprintf(f, "<TR>\n");
for (i=0; i<stepCount; ++i)
    {
    struct rgbColor col = saturatedRainbowAtPos(rainbowPos);
    fprintf(f, "<TD BGCOLOR='#%02X%02X%02X'>&nbsp;</TD>\n",
            col.r, col.g, col.b);
    rainbowPos += colorStep;
    }
fprintf(f, "</TR>\n");
fprintf(f, "</TABLE>\n");
fprintf(f, "<BR><BR>\n");


/* Finally just print out the one we like, the last one, as a C structure. */
fprintf(f, "And the last 3 columns in C<BR>");
fprintf(f, "<pre><tt>");
fprintf(f, "struct rgbColor lightRainbow[30] = {\n");
pt = init_cmap + rainStart * 3;
for (colorIx = rainStart; colorIx < rainEnd; ++colorIx)
    {
    int rawR = pt[0]*4, rawG = pt[1]*4, rawB = pt[2]*4;
    pt += 3;
    /* Show raw colors mixed 50% with white */
    int lightR = (rawR + 255)/2, lightG = (rawG + 255)/2, lightB = (rawB + 255)/2;
    int oneBright=brightness(lightR, lightG, lightB);
    double scale = prettyBright/oneBright;
    int r = lightR*scale, g = lightG*scale, b = lightB*scale;
    fprintf(f, "    {%d, %d, %d},\n", r, g, b);
    }
fprintf(f, "    };\n");
fprintf(f, "</tt></pre>");

/* Also, what the heck, print out the fully saturated one with the varying brightness. */
fprintf(f, "And the vivid rainbow columns in C<BR>");
fprintf(f, "<pre><tt>");
fprintf(f, "struct rgbColor saturatedRainbow[30] = {\n");
pt = init_cmap + rainStart * 3;
for (colorIx = rainStart; colorIx < rainEnd; ++colorIx)
    {
    int r = pt[0]*4, g = pt[1]*4, b = pt[2]*4;
    pt += 3;
    fprintf(f, "    {%d, %d, %d},\n", r, g, b);
    }
fprintf(f, "    };\n");
fprintf(f, "</tt></pre>");


htmEnd(f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
// optionInit(&argc, argv, options);

if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
