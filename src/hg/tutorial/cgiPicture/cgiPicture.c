/* cgiPicture - Simple CGI that makes an image.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "portable.h"
#include "hui.h"
#include "vGfx.h"

static char const rcsid[] = "$Id: cgiPicture.c,v 1.2.44.1 2008/08/01 06:10:57 markd Exp $";

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;

char *shapes[] = {
   "square",
   "line",
   "X",
};

#define varPrefix "cgiPicture."
#define shapeVar  varPrefix "shape"
#define redVar varPrefix "red"
#define greenVar varPrefix "green"
#define blueVar varPrefix "blue"

void drawImage(struct vGfx *vg)
/* Draw on our image. */
{
int red = cartInt(cart, redVar);
int green = cartInt(cart, greenVar);
int blue = cartInt(cart, blueVar);
char *shape = cartString(cart, shapeVar);
Color col = vgFindColorIx(vg, red, green, blue);
if (sameString(shape, "square"))
    {
    /* Draw a square half the dimensions of image in middle */
    int h2 = vg->height/2;
    int w2 = vg->width/2;
    vgBox(vg, w2/2, h2/2, w2, h2, col);
    }
else if (sameString(shape, "line"))
    {
    vgLine(vg, 0, 0, vg->width, vg->height, col);
    }
else if (sameString(shape, "X"))
    {
    vgLine(vg, 0, 0, vg->width, vg->height, col);
    vgLine(vg, vg->width, 0, 0, vg->height, col);
    }
else
    {
    errAbort("Unknown shape %s", shape);
    }
#ifdef NEVER
#endif /* NEVER */
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;
cartWebStart(cart, database, "Simple CGI that makes an image.");
printf("<FORM ACTION=\"../cgi-bin/cgiPicture\">");
cartSaveSession(cart);

/* Put up some web controls. */
cgiMakeDropList(shapeVar, shapes, ArraySize(shapes), 
	cartUsualString(cart, shapeVar, shapes[0]));
cgiMakeSubmitButton();
printf("<BR>");
printf("Color.  Red: ");
cgiMakeIntVar(redVar, cartUsualInt(cart, redVar, 200), 3 );
printf(" Green: ");
cgiMakeIntVar(greenVar, cartUsualInt(cart, greenVar, 200), 3 );
printf(" Blue: ");
cgiMakeIntVar(blueVar, cartUsualInt(cart, blueVar, 200), 3 );
printf("<BR>");


/* Create and draw image to temp file and write out URL of temp file. */
struct tempName tn;
makeTempName(&tn, "image", ".gif");
struct vGfx *vg = vgOpenGif(500, 500, tn.forCgi);
drawImage(vg);
vgClose(&vg);
printf("<IMG SRC=\"%s\">", tn.forCgi);

printf("</FORM>");
cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
