/* hgGeneRing - Gene Network Browser. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "obscure.h"
#include "web.h"
#include "cart.h"
#include "hdb.h"
#include "dbDb.h"
#include "hgFind.h"
#include "hCommon.h"
#include "hui.h"

#include "interaction.h"
#include "portable.h"

static char const rcsid[] = "$Id: hgGeneRing.c,v 1.1 2005/02/13 10:46:42 galt Exp $";


struct interaction *interactions = NULL;

struct node {
    char *name;               /* name or id - use also as hash key */
    struct nodelist *xrays;   /* out-going rays in directed graph  */
    struct nodelist *nrays;   /* in-coming rays in directed graph  */
    boolean ring;             /* member of gene-ring? */
    int ringRank;             /* number of ring members connected to by x or n rays */
    int xpos;                 /* x position */
    int ypos;                 /* y position */
};


struct nodelist {
    struct nodelist *next;  /* next in list */
    struct node *node;      
};

struct nodelist* allNodes = NULL;
struct hash *nodeHash = NULL;

int ringCount = 0;
int insideCount = 0;
int outsideCount = 0;

struct cart *cart = NULL;
struct hash *oldVars = NULL;
char *clade = NULL;
char *organism = NULL;
char *db = NULL;

/*
  Remove any custom track data from the cart.
*/
void removeCustomTrackData()
{
cartRemove(cart, "hgt.customText");
cartRemove(cart, "hgt.customFile");
cartRemove(cart, "ct");
}

void hgGateway()
/* hgGateway - Human Genome Browser Gateway. */
{
char *oldDb = NULL;
char *oldOrg = NULL;
char *oldClade = NULL;
char *defaultPosition = hDefaultPos(db);
char *position = cloneString(cartUsualString(cart, "position", defaultPosition));
boolean gotClade = hGotClade();

/* JavaScript to copy input data on the change genome button to a hidden form
This was done in order to be able to flexibly arrange the UI HTML
*/
char *onChangeDB = "onchange=\"document.orgForm.db.value = document.mainForm.db.options[document.mainForm.db.selectedIndex].value; document.orgForm.submit();\"";
char *onChangeOrg = "onchange=\"document.orgForm.org.value = document.mainForm.org.options[document.mainForm.org.selectedIndex].value; document.orgForm.db.value = 0; document.orgForm.submit();\"";
char *onChangeClade = "onchange=\"document.orgForm.clade.value = document.mainForm.clade.options[document.mainForm.clade.selectedIndex].value; document.orgForm.org.value = 0; document.orgForm.db.value = 0; document.orgForm.submit();\"";

/* 
   If we are changing databases via explicit cgi request,
   then remove custom track data which will 
   be irrelevant in this new database .
   If databases were changed then use the new default position too.
*/

oldDb = hashFindVal(oldVars, "db");
oldOrg = hashFindVal(oldVars, "org");
oldClade = hashFindVal(oldVars, "clade");
if ((oldDb    && differentWord(oldDb, db)) ||
    (oldOrg   && differentWord(oldOrg, organism)) ||
    (oldClade && differentWord(oldClade, clade)))
    {
    position = defaultPosition;
    removeCustomTrackData();
    }
if (sameString(position, "genome") || sameString(position, "hgBatch"))
    position = defaultPosition;

puts(
"<FORM ACTION=\"/cgi-bin/hgTracks\" NAME=\"mainForm\" METHOD=\"GET\">\n"
"<CENTER>"
"<TABLE BGCOLOR=\"FFFEF3\" BORDERCOLOR=\"cccc99\" BORDER=0 CELLPADDING=1>\n"
"<TR><TD><FONT SIZE=\"2\">\n"
"<CENTER>\n"
"The UCSC Genome Browser was created by the \n"
"<A HREF=\"/staff.html\">Genome Bioinformatics Group of UC Santa Cruz</A>.\n"
"<BR>"
"Software Copyright (c) The Regents of the University of California.\n"
"All rights reserved.\n"
"</CENTER>\n"
"</FONT></TD></TR></TABLE></CENTER>\n"
);

puts(
"<input TYPE=\"IMAGE\" BORDER=\"0\" NAME=\"hgt.dummyEnterButton\" src=\"/images/DOT.gif\" WIDTH=1 HEIGHT=1 ALT=dot>\n"
"<center>\n"
"<table bgcolor=\"cccc99\" border=\"0\" CELLPADDING=1 CELLSPACING=0>\n"
"<tr><td>\n"
"<table BGCOLOR=\"FEFDEF\" BORDERCOLOR=\"CCCC99\" BORDER=0 CELLPADDING=0 CELLSPACING=0>\n"  
"<tr><td>\n"
"<table bgcolor=\"fffef3\" border=0>\n"
"<tr>\n"
"<td>\n"
"<table><tr>");
if (gotClade)
    puts("<td align=center valign=baseline>clade</td>");
puts(
"<td align=center valign=baseline>genome</td>\n"
"<td align=center valign=baseline>assembly</td>\n"
"<td align=center valign=baseline>position</td>\n"
"<td align=center valign=baseline>image width</td>\n"
"<td align=center valign=baseline> &nbsp; </td>\n"
"</tr>\n<tr>"
);

if (gotClade)
    {
    puts("<td align=center>\n");
    printCladeListHtml(organism, onChangeClade);
    puts("</td>\n");
    }

puts("<td align=center>\n");
if (gotClade)
    printGenomeListForCladeHtml(db, onChangeOrg);
else
    printGenomeListHtml(db, onChangeOrg);
puts("</td>\n");

puts("<td align=center>\n");
/*   HACK ALERT - Zoo needs to have different pulldown behavior - Hiram */
if ( startsWith( "Zoo", organism ) ) {
puts("<select NAME=\"db\" onchange=\"document.orgForm.db.value = document.mainForm.db.options[document.mainForm.db.selectedIndex].value; document.orgForm.submit();\">"
    "\t<option SELECTED VALUE=\"zooHuman3\">June 2002</option>"
    "\t</select>");
} else {
    printAssemblyListHtml(db, onChangeDB);
}
puts("</td>\n");

puts("<td align=center>\n");
cgiMakeTextVar("position", addCommasToPos(position), 30);
printf("</td>\n");

#ifdef SORRY_GILL_I_HIT_INSTEAD_OF_SUBMIT_TOO_MANY_TIMES
puts("<td align=center>\n");
cgiMakeOnClickButton("document.mainForm.position.value=''","clear");
printf("</td>\n");
#endif /* SORRY_GILL_I_HIT_INSTEAD_OF_SUBMIT_TOO_MANY_TIMES */


cartSetString(cart, "position", position);
cartSetString(cart, "db", db);
cartSetString(cart, "org", organism);
if (gotClade)
    cartSetString(cart, "clade", clade);

freez(&defaultPosition);
position = NULL;

puts("<td align=center>\n");
cgiMakeIntVar("pix", cartUsualInt(cart, "pix", hgDefaultPixWidth), 4);
cartSaveSession(cart);
printf("</td>\n");
printf("<td align=center>");
cgiMakeButton("Submit", "Submit");
printf("</td>\n");

puts(
"</tr></table>\n"
"</td></tr><tr><td><center>\n"
"<a HREF=\"../cgi-bin/cartReset\">Click here to reset</a> the browser user interface settings to their defaults.<BR>\n"
"</center>\n"
"</td></tr><tr><td><center>\n"
);
cgiMakeButton("customTrackPage", "Add Your Own Custom Tracks");
puts("</center>\n"
"</td></tr></table>\n"
"</td></tr></table>\n"
"</td></tr></table>\n"
);
puts("</center>");

hgPositionsHelpHtml(organism, db);

puts("</FORM>\n"
);

puts("<FORM ACTION=\"/cgi-bin/hgGateway\" METHOD=\"GET\" NAME=\"orgForm\">");
if (gotClade)
    printf("<input type=\"hidden\" name=\"clade\" value=\"%s\">\n", clade);
printf("<input type=\"hidden\" name=\"org\" value=\"%s\">\n", organism);
printf("<input type=\"hidden\" name=\"db\" value=\"%s\">\n", db);
cartSaveSession(cart);
puts("</FORM><BR>");
}



struct interaction* getGenesFromTable(char* table, char* geneString)
/* read all the gene interactions from database, returns interaction-list */
{
struct interaction* result = NULL;
struct dyString *query=newDyString(512);
char* sep = "where";
char* ss = cloneString(geneString);
char* s = ss;
char* w = NULL;
subChar(s, ',' ,' ');
subChar(s, '\t',' ');
dyStringPrintf(query,"select * from %s",table);
while(1)
    {
    struct hashEl *hel = NULL;
    if (!(w = nextWord(&s))) break;
    hel = hashStore(nodeHash,w);
    if (!hel->val)                /* add geneList elements to the hash */
	{
	struct node *n;
	struct nodelist *nl;
	AllocVar(n);
	AllocVar(nl);
	n->name = cloneString(w);
	n->ring = TRUE;          /* this is a special ring member */
	nl->node = n;
	slAddHead(&allNodes,nl); /* all nodes going in hash are added to all-nodes list */
	hel->val = n;
	}
    dyStringPrintf(query," %s fromX='%s' or toY='%s'",sep,w,w);
    sep = "or";
    }
//uglyf("<br>SQL=%s<br><br>\n",query->string);    
struct sqlConnection* conn = hAllocConn();
result = interactionLoadByQuery(conn, query->string);
hFreeConn(&conn);
freeDyString(&query);
freez(&ss);
return result;
}


void getGeneList()
/* hgGeneRing - Gene Network Browser. */
{
char *geneList = cloneString(cartUsualString(cart, "ring_geneList", ""));

/* not much point in this, though called old it's the same as current
   at this point.  It is not the old value from before the last submit.
char *oldGeneList = hashFindVal(oldVars, "ring_geneList");
*/

puts(
"<FORM ACTION=\"/cgi-bin/hgGeneRing\" NAME=\"mainForm\" METHOD=\"GET\">\n"
"<CENTER>"
"<TABLE BGCOLOR=\"FFFEF3\" BORDERCOLOR=\"cccc99\" BORDER=0 CELLPADDING=1>\n"
"<TR><TD><FONT SIZE=\"2\">\n"
"<CENTER>\n"
"Enter gene list for gene network ring.\n"
"</CENTER>\n"
"</FONT></TD></TR></TABLE></CENTER>\n"
);

puts(
"<center>"
"<table bgcolor=\"cccc99\" border=\"0\" CELLPADDING=1 CELLSPACING=0>\n"
"<tr>\n"
);

cartSetString(cart, "ring_action", "saveGeneList");

puts("<td align=center>\n");
cgiMakeTextArea("ring_geneList", geneList, 25, 30);
printf("</td>\n");

puts(
"</tr><tr>\n"
);

printf("<td align=center>");
cgiMakeButton("Submit", "Submit");
printf("</td>\n");

puts(
"</tr></table></center>\n"
);
puts("</FORM>\n");

freez(&geneList);
}


int compareNodes(const void *va, const void *vb)
{
const struct nodelist *a = *((struct nodelist **)va);
const struct nodelist *b = *((struct nodelist **)vb);
if (a->node->ring && !b->node->ring) return -1;
if (!a->node->ring && b->node->ring) return  1;
if (a->node->ring && b->node->ring)
    {
    return differentWord(a->node->name,b->node->name)*-1; 
    }
if (a->node->ringRank == b->node->ringRank)
    {
    return differentWord(a->node->name,b->node->name)*-1; 
    }
else
    {
    return (b->node->ringRank - a->node->ringRank); 
    }
}

void saveGeneList(boolean showAll)
/* Check for valid gene-list: parse and save.
   If list not ok, return to getGeneList page.
   Otherwise, show user's list and offer some links.
*/
{
char *geneList = cloneString(cartUsualString(cart, "ring_geneList", ""));
struct interaction* intr;
if (sameWord(geneList,""))
    {
    getGeneList();
    }
else
    {
    //action = "getGeneList";
    // cartSetString(cart, "ring_action", action);

    if (showAll)
	{
	puts(
	"<center>"
	"<table bgcolor=\"cccc99\" border=\"0\" CELLPADDING=1 CELLSPACING=0>\n"
	"<tr><td>\n"
	);
	printf("geneList: %s<br>\n", 
	    geneList
	);
	puts(
	"</td></tr><tr><td>"
	"<a href=\"/cgi-bin/hgGeneRing?ring_action=getGeneList\">back</a>\n"
	"</td></tr><tr><td>"
	"<a href=\"/cgi-bin/hgGeneRing?ring_action=drawScreen\">screen</a>\n"
	);
	puts(
	"</td></tr></table></center>\n"
	);

	}
    
    interactions = getGenesFromTable("intrP2P",geneList);  /* adds geneList elements to hash */
    
    if (showAll)
	{
	uglyf("slCount = %d for result list for %s.<br><br>\n",slCount(interactions),geneList);
	}
    
    /* this might get moved later */
    /* add to hash the remaining interactions members */
    for(intr=interactions;intr;intr=intr->next)
	{
	//uglyf("%s %s %f.<br>\n",intr->fromX,intr->toY,intr->score);
	struct hashEl *helX = hashStore(nodeHash,intr->fromX);
	struct hashEl *helY = hashStore(nodeHash,intr->toY);
    	struct node *x, *y;
	struct nodelist *nl;
	if (!helX->val) /* add X to hash for 1st time */
	    {
	    AllocVar(x);
	    AllocVar(nl);
	    x->name = cloneString(intr->fromX);
	    nl->node = x;
	    slAddHead(&allNodes,nl);  /* add X to list of all nodes */
	    helX->val = x;
	    }
	if (!helY->val) /* do same stuff for Y */
	    {
	    AllocVar(y);
	    AllocVar(nl);
	    y->name = cloneString(intr->toY);
	    nl->node = y;
	    slAddHead(&allNodes,nl);
	    helY->val = y;
	    }
	x = helX->val;  /* get X,Y from hash */
	y = helY->val;
	if (x->ring) y->ringRank++;  /* increment ringRank counts */
	if (y->ring) x->ringRank++;
    	AllocVar(nl);
    	nl->node = y;
    	slAddHead(&x->xrays,nl);     /* add Y to X's list of x-rays  */
    	AllocVar(nl);
    	nl->node = x;
    	slAddHead(&y->nrays,nl);     /* likewise */
	}
   
    interactionFreeList(&interactions);

    slReverse(&allNodes);

    slSort(&allNodes, compareNodes);


    {
    // displaying our structure 
    struct nodelist *nl;
    if (showAll)
	{
    	uglyf("<pre>\n");
	uglyf("name        xrays nrays  ring? ringRank \n");
    	uglyf("-------------------------------------- \n");
	}
    for(nl=allNodes;nl;nl=nl->next)
	{

        if (nl->node->ring)
	    {
    	    ringCount++;
	    }
	else
	    {
	    if (nl->node->ringRank > 1)
		{
		insideCount++;
		}
	    else
		{
		outsideCount++;
		}
	    }
	
	if (showAll)
	    {
	    uglyf("%11s %4d %4d     %4s %3d \n",
		nl->node->name,
		slCount(nl->node->xrays),
		slCount(nl->node->nrays),
		nl->node->ring ? "ring":"not!",
		nl->node->ringRank
		);
	    }
	}
    
    if (showAll)
	{
	uglyf("</pre>\n");
	}

    }
    
    // TODO: add some real code for freeing these things properly
    //   e.g. for xrays and nrays lists, free up the nodelist list itself,
    //     but not the *node values themselves, which are allocated
    //     in the main nodelist/hash.  If the nodelist is freed up
    //     properly then hash freeing should be easy enough.
    //     Do we use valgrind or some other util to verify mem-release?

   

    }

freez(&geneList);
}


void drawCircle()
/*
 just a test of drawing a circle .gif 
 and displaying it in a page.
*/
{
struct tempName filename;
int width,height;
char *mapName="ring_map";

ZeroVar(&filename);
makeTempName(&filename, "ring_circleTest", ".gif");

struct memGfx *mg = mgNew(800,800);
int i;
int xCen = 400, yCen = 400;
Color color = MG_RED;
for (i=800; i>= 400; i -= 40)
    {
    mgCircle(mg, xCen, yCen, i, color, TRUE);
    if (color == MG_RED)
	color = MG_BLUE;
    else
	color = MG_RED;
    }
for (i=360; i >= 0; i -= 20)
    mgCircle(mg, xCen, yCen, i, MG_WHITE, FALSE);

mgSaveGif(mg, filename.forCgi);
 
width=height=800;
printf("<CENTER><TABLE BORDER=0 CELLPADDING=0>");
printf("<TR><TD HEIGHT=5></TD></TR>");
printf("<TR><TD><IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s >",
       filename.forHtml, width, height, mapName);
printf("</TD></TR>");
printf("<TR><TD HEIGHT=5></TD></TR>");
printf("<TR><TD><a href = \"/cgi-bin/hgGeneRing?ring_action=saveGeneList\">back</a>");
printf("</TD></TR>");
printf("<TR><TD HEIGHT=5></TD></TR></TABLE></CENTER>");
			    
}

void drawScreen()
/*
 Draw Gene Ring using given genes and interactions
 and display it in a page as a .gif
*/
{
struct tempName filename;
int width,height;
char *mapName="ring_map";

ZeroVar(&filename);
makeTempName(&filename, "hgGeneRing_screen", ".gif");

struct memGfx *mg = mgNew(800,800);
int i;
double angle=0, subangle=0;
int xCen = 400, yCen = 400;
int x=0,y=0;
struct nodelist *nl;

/* draw the main RING */
mgCircle(mg, xCen, yCen, 200, MG_BLACK, FALSE);

/* draw the RING genes and assign position */
angle=(M_PI*2)/ringCount;
nl=allNodes;
for(i=0;i<ringCount;i++)
    {
    x = xCen + (int) 200*cos(i*angle);
    y = yCen + (int) 200*sin(i*angle);
    nl->node->xpos = x;
    nl->node->ypos = y;
    mgCircle(mg, x, y, 10, MG_BLACK, FALSE);
    nl=nl->next;
    }

/* draw the Inside genes, assign position, and draw rays */
angle=(M_PI*2)/insideCount;
for(i=0;i<insideCount;i++)
    {
    x = xCen + (int) 140*cos((i+.5)*angle);
    y = yCen + (int) 140*sin((i+.5)*angle);
    nl->node->xpos = x;
    nl->node->ypos = y;
    mgCircle(mg, x, y, 10, MG_BLACK, FALSE);
    
    /* draw connections to ring */
    { /*local*/
    struct nodelist *l;
    for(l=nl->node->xrays;l;l=l->next)
	{
	mgDrawLine(mg, x, y, l->node->xpos, l->node->ypos, MG_BLUE);
	}
    for(l=nl->node->nrays;l;l=l->next)
	{
	mgDrawLine(mg, x, y, l->node->xpos, l->node->ypos, MG_RED);
	}
    }
    
    nl=nl->next;
    }


/* draw the Outside genes, assign position, draw rays */
angle=(M_PI*2)/ringCount;
for(i=0;i<outsideCount;i++)
    {
    x = xCen + (int) 140*cos((i+.5)*angle);
    y = yCen + (int) 140*sin((i+.5)*angle);
    nl->node->xpos = x;
    nl->node->ypos = y;
    mgCircle(mg, x, y, 10, MG_BLACK, FALSE);
    
    /* draw connections to ring */
    { /*local*/
    struct nodelist *l;
    for(l=nl->node->xrays;l;l=l->next)
	{
	mgDrawLine(mg, x, y, l->node->xpos, l->node->ypos, MG_BLUE);
	}
    for(l=nl->node->nrays;l;l=l->next)
	{
	mgDrawLine(mg, x, y, l->node->xpos, l->node->ypos, MG_RED);
	}
    }
    
    nl=nl->next;
    }



mgSaveGif(mg, filename.forCgi);
 
width=height=800;
printf("<CENTER><TABLE BORDER=0 CELLPADDING=0>");
printf("<TR><TD HEIGHT=5></TD></TR>");
printf("<TR><TD><IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s >",
       filename.forHtml, width, height, mapName);
printf("</TD></TR>");
printf("<TR><TD HEIGHT=5></TD></TR>");
printf("<TR><TD><a href = \"/cgi-bin/hgGeneRing?ring_action=saveGeneList\">back</a>");
printf("</TD></TR>");
printf("<TR><TD HEIGHT=5></TD></TR></TABLE></CENTER>");
			    
}


void hgGeneRing()
/* hgGeneRing - Gene Network Browser. */
{
char *action = cartUsualString(cart, "ring_action", "");
if (sameWord(action,""))
    {
    action = "getGeneList";
    cartSetString(cart, "ring_action", action);
    }

if (sameWord(action,"getGeneList"))
    {
    getGeneList();
    }
else if (sameWord(action,"saveGeneList"))
    {
    saveGeneList(TRUE);
    }
else if (sameWord(action,"drawScreen"))
    {
    saveGeneList(FALSE);
    drawScreen();
    }
else
    {
    getGeneList();
    }

}

void doMiddle(struct cart *theCart)
/* Set up pretty web display and save cart in global. */
{
cart = theCart;

getDbGenomeClade(cart, &db, &organism, &clade);
if (! hDbIsActive(db))
    {
    db = hDefaultDb();
    organism = hGenome(db);
    clade = hClade(organism);
    }

/* temporary hack, set db to dm1 fly */
db = "dm1";
organism = "D. melanogaster";
clade = "insect";
cartSetString(cart, "db", db);
cartSetString(cart, "org", organism);
cartSetString(cart, "clade", clade);

hSetDb(db);

cartWebStart(theCart, "%s Gene Network Browser \n", organism);

hgGeneRing();

cartSaveSession(cart);
cartWebEnd();
}

char *excludeVars[] = {NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(8);
cgiSpoof(&argc, argv);

nodeHash = newHash(8);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
freeHashAndVals(&nodeHash);
return 0;
}

