/* freen - My Pet Freen. */
#include <unistd.h>
#include <math.h>

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "memgfx.h"

#define TRAVERSE FALSE

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}




int demoFont(struct memGfx *mg, MgFont *font, char *name, int x, int y)
/* Print out some text in font.  Return text height. */
{
char buf[64];
safef(buf, sizeof(buf), "abcde123ABCDExyz %s", name);
mgText(mg, x, y, MG_BLACK, font, buf);
return mgFontLineHeight(font);
}

void freen(char *a)
/* Test some hair-brained thing. */
{
struct memGfx *mg = mgNew(1000,1000);
int y = 5;

y += demoFont(mg, mgTinyFont(), "sixhi 6", 5, y);
y += demoFont(mg, mgSmallFont(), "sail 8", 5, y);
y += demoFont(mg, mgCourier8Font(), "courier 8", 5, y);
y += demoFont(mg, mgCourier10Font(), "courier 10", 5, y);
y += demoFont(mg, mgCourier12Font(), "courier 12", 5, y);
y += demoFont(mg, mgCourier14Font(), "courier 14", 5, y);
y += demoFont(mg, mgCourier18Font(), "courier 18", 5, y);
y += demoFont(mg, mgCourier24Font(), "courier 24", 5, y);
y += demoFont(mg, mgCourier34Font(), "fixed 34", 5, y);
y += demoFont(mg, mgHelvetica8Font(), "helvetica 8", 5, y);
y += demoFont(mg, mgHelvetica10Font(), "helvetica 10", 5, y);
y += demoFont(mg, mgHelvetica12Font(), "helvetica 12", 5, y);
y += demoFont(mg, mgHelvetica14Font(), "helvetica 14", 5, y);
y += demoFont(mg, mgHelvetica18Font(), "helvetica 18", 5, y);
y += demoFont(mg, mgHelvetica24Font(), "helvetica 24", 5, y);
y += demoFont(mg, mgHelvetica34Font(), "helvetica 34", 5, y);
y += demoFont(mg, mgHelveticaBold8Font(), "helvetica bold 8", 5, y);
y += demoFont(mg, mgHelveticaBold10Font(), "helvetica bold 10", 5, y);
y += demoFont(mg, mgHelveticaBold12Font(), "helvetica bold 12", 5, y);
y += demoFont(mg, mgHelveticaBold14Font(), "helvetica bold 14", 5, y);
y += demoFont(mg, mgHelveticaBold18Font(), "helvetica bold 18", 5, y);
y += demoFont(mg, mgHelveticaBold24Font(), "helvetica bold 24", 5, y);
y += demoFont(mg, mgHelveticaBold34Font(), "helvetica bold 34", 5, y);
y += demoFont(mg, mgTimes8Font(), "times 8", 5, y);
y += demoFont(mg, mgTimes10Font(), "times 10", 5, y);
y += demoFont(mg, mgTimes12Font(), "times 12", 5, y);
y += demoFont(mg, mgTimes14Font(), "times 14", 5, y);
y += demoFont(mg, mgTimes18Font(), "times 18", 5, y);
y += demoFont(mg, mgTimes24Font(), "times 24", 5, y);
mgSaveGif(mg, a, FALSE);
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
