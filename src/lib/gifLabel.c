/* gifLabel - create labels as GIF files. */

#include "common.h"
#include "memgfx.h"
#include "gifLabel.h"

static char const rcsid[] = "$Id: gifLabel.c,v 1.7 2005/06/06 20:20:10 galt Exp $";

int gifLabelMaxWidth(char **labels, int labelCount)
/* Return maximum pixel width of labels.  It's ok to have
 * NULLs in labels array. */
{
int width = 0, w, i;
MgFont *font = mgMediumFont();
for (i=0; i<labelCount; ++i)
    {
    char *label = labels[i];
    if (label != NULL)
	{
	w = mgFontStringWidth(font, labels[i]);
	if (w > width)
	    width = w;
	}
    }
width += 2;
return width;
}

static struct memGfx *altColorLabels(char **labels, int labelCount, int width)
/* Return a memory image with alternating colors. */
{
struct memGfx *mg = NULL;
Color c1,c2;
MgFont *font = mgMediumFont();
int lineHeight = mgFontLineHeight(font)-1;
int height = lineHeight * labelCount, i;
int y = 0;

/* Allocate picture and set up colors. */
mg = mgNew(width, height);
c1 = mgFindColor(mg, 0xE0, 0xE0, 0xFF);
c2 = mgFindColor(mg, 0xFF, 0xC8, 0xC8);

/* Draw text. */
for (i=labelCount-1; i >= 0; --i)
    {
    Color c = ((i&1) ? c2 : c1);
    mgDrawBox(mg, 0, y, width, lineHeight, c);
    mgTextRight(mg, 0+1, y+1, width-1, lineHeight, MG_BLACK, font, labels[i]);
    y += lineHeight;
    }

return mg;
}


boolean sameFileContents(char *n1, char *n2)
/* compare two files and return true if their contents are identical using binary compare */
{
int r1 = 0, r2 = 0;
char buf1[4096];
char buf2[4096];
FILE *f1 = NULL, *f2 = NULL;
boolean result = TRUE;
f1 = fopen(n1,"r"); if (f1 == NULL) { return FALSE; }
f2 = fopen(n2,"r"); if (f2 == NULL) { fclose(f1); return FALSE; }
while (TRUE)
    {
    r1 = fread(buf1, 1, sizeof(buf1), f1);
    r2 = fread(buf2, 1, sizeof(buf2), f2);
    if (r1 != r2) { result = FALSE; break; } /* diff file size */
    if (r1 == 0) { break; }  /* eof */
    if (memcmp(buf1,buf2,r1)!=0) { result = FALSE; break; } /* file contents differ */
    }
fclose(f2);
fclose(f1);
return result;
}


void gifLabelVerticalText(char *fileName, char **labels, int labelCount, 
	int height)
/* Make a gif file with given labels.  This will check to see if fileName
 * exists already, and if so do nothing. */
{
struct memGfx *straight = altColorLabels(labels, labelCount, height);
struct memGfx *rotated = mgRotate90(straight);
char tempName[512];
safef(tempName, sizeof(tempName), "%s_trash", fileName);
mgSaveGif(rotated, tempName);  /* note we cannot delete this later because of permissions */
/* the savings here is in the user's own browser cache - not updated if no change */
if (!sameFileContents(tempName,fileName))  
    mgSaveGif(rotated, fileName);
mgFree(&straight);
mgFree(&rotated);
}


#ifdef DEBUG
void gifTest()
{
static char *labels[] = {"cerebellum", "thymus", "breast", "heart",
			 "stomach", "cartilage", "kidney", "liver",
			 "lung", "testis", "black hole" };
int size = gifLabelMaxWidth(labels, ArraySize(labels));
int gifLabelMaxWidth(char **labels, int labelCount)
gifLabelVerticalText("../trash/foo.gif", labels, ArraySize(labels), size);
uglyf("<IMG SRC=\"../trash/foo.gif\">");
}
#endif /* DEBUG */
