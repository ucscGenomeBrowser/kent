/* counter.c - A simple web hit counter. */
#include "common.h"
#include <time.h>
#include "cheapcgi.h"
#include "memgfx.h"
#include "obscure.h"


struct memGfx *makeCountPic(long count, MgFont *font)
{
char text[16];
int textWidth, textHeight;
int pixWidth, pixHeight;
struct memGfx *mg;

sprintf(text, "%ld", count);
textWidth = mgFontStringWidth(font, text);
textHeight = mgFontLineHeight(font);
pixWidth = textWidth + 4;
pixHeight = textHeight + 4;
mg = mgNew(pixWidth, pixHeight);
mgClearPixels(mg);
mgTextCentered(mg, 0, 0, pixWidth, pixHeight, MG_BLACK, font, text);
return mg;
}

long incCount(char *fileName, boolean saveWhence)
/* Increment counter at start of file.  Add hit to end of file. 
 * Return count. */
{
long val = 0;
FILE *f = fopen(fileName, "r+b");
char *ip;
int ipSize;
time_t seconds = time(NULL);
ip = getenv("REMOTE_HOST");
if (ip == NULL || ip[0] == 0)
    ip = getenv("REMOTE_ADDR");
if (ip == NULL)
    ip = "";
ipSize = strlen(ip) + 1;

if (f != NULL)
    {
    mustReadOne(f, val);
    rewind(f);
    }
else
    {
    f = fopen(fileName, "wb");
    }
++val;
if (f != NULL)
    {
    fwrite(&val, sizeof(val), 1, f);
    fseek(f, 0L, SEEK_END);
    fwrite(&seconds, sizeof(seconds), 1, f);
    fwrite(ip, ipSize, 1, f);
    if (saveWhence)
        {
        char *whence = getenv("HTTP_REFERER");
        int whenceSize;
        if (whence == NULL)
            whence = "";
        whenceSize = strlen(whence)+1;
        fwrite(whence, whenceSize, 1, f);
        }
    }
return val;
}

int main(int argc, char *argv[])
{
char *counterFileName;
long count;
struct memGfx *mg;
boolean saveWhence = cgiBoolean("whence");
boolean mute = cgiBoolean("mute");

counterFileName = cgiOptionalString("file");
if (counterFileName == NULL)
    counterFileName = "default.ctr";
count = incCount(counterFileName, saveWhence);

if (mute)
    mg = mgNew(1, 1);
else
    mg = makeCountPic(count, mgMediumFont());
fprintf(stdout, "Content-type: image/png\n\n");
mgSaveToPng(stdout, mg, FALSE);
return 0;
}
