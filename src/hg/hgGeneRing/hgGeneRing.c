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
#include "bdgpGeneInfo.h"
#include "portable.h"

static char const rcsid[] = "$Id: hgGeneRing.c,v 1.8 2009/08/19 23:25:19 angie Exp $";

char *errMsg = NULL;

char *geneList = NULL;

struct slName *geneNames = NULL;
struct hash *aliasHash = NULL;

struct interaction *interactions = NULL;

struct node {
    char *name;               /* name or id - use also as hash key */
    char *alias;              /* if used */
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


/* ------ old junk cloned from hgGateway --------- */

void hgGateway()
/* hgGateway - Human Genome Browser Gateway. */
{
char *defaultPosition = hDefaultPos(db);
char *position = cloneString(cartUsualString(cart, "position", defaultPosition));
boolean gotClade = hGotClade();

/* JavaScript to copy input data on the change genome button to a hidden form
This was done in order to be able to flexibly arrange the UI HTML
*/
char *onChangeDB = "onchange=\"document.orgForm.db.value = document.mainForm.db.options[document.mainForm.db.selectedIndex].value; document.orgForm.submit();\"";
char *onChangeOrg = "onchange=\"document.orgForm.org.value = document.mainForm.org.options[document.mainForm.org.selectedIndex].value; document.orgForm.db.value = 0; document.orgForm.submit();\"";
char *onChangeClade = "onchange=\"document.orgForm.clade.value = document.mainForm.clade.options[document.mainForm.clade.selectedIndex].value; document.orgForm.org.value = 0; document.orgForm.db.value = 0; document.orgForm.submit();\"";

if (sameString(position, "genome") || sameString(position, "hgBatch"))
    position = defaultPosition;

puts(
"<FORM ACTION=\"/cgi-bin/hgTracks\" NAME=\"mainForm\" METHOD=\"GET\">\n"
"<CENTER>"
"<TABLE CELLPADDING=1 style='background-color:#FFFEF3; border-style:none;'>\n"
"<TR><TD style='text-align:center; font-size:small;'>\n"
"The UCSC Genome Browser was created by the \n"
"<A HREF=\"/staff.html\">Genome Bioinformatics Group of UC Santa Cruz</A>.\n"
"<BR>"
"Software Copyright (c) The Regents of the University of California.\n"
"All rights reserved.\n"
"</TD></TR></TABLE></CENTER>\n"
);

puts(
"<input TYPE=\"IMAGE\" BORDER=\"0\" NAME=\"hgt.dummyEnterButton\" src=\"/images/DOT.gif\" WIDTH=1 HEIGHT=1 ALT=dot>\n"
"<CENTER>\n"
"<table bgcolor='#CCCC99' border=0 CELLPADDING=1 CELLSPACING=0>\n"
"<tr><td>\n"
"<table CELLPADDING=0 CELLSPACING=0 style='background-color:#FEFDEF; border-style:none;'>\n"
"<tr><td>\n"
"<table bgcolor='#FFFEF3' border=0>\n"
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
"</td></tr><tr><td><CENTER>\n"
"<a HREF=\"../cgi-bin/cartReset\">Click here to reset</a> the browser user interface settings to their defaults.<BR>\n"
"</CENTER>\n"
"</td></tr><tr><td><CENTER>\n"
);
cgiMakeButton("customTrackPage", "Add Your Own Custom Tracks");
puts("</CENTER>\n"
"</td></tr></table>\n"
"</td></tr></table>\n"
"</td></tr></table>\n"
);
puts("</CENTER>");

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

/* end of hgGateway junk that this code was cloned from */


/* ----------------------------------------------------------------------- */

boolean getGenesAsList()
/* create name-list from geneString,
   so that we don't have to keep re-parsing
   the cart string in different places
*/
{
char* ss = cloneString(geneList);
char* s = ss;
char* w = NULL;
int c=0;
char emsg[256];
/* clean up unwanted characters if any */
subChar(ss, ',' ,' ');
subChar(ss, '\t',' ');
subChar(ss, '\n',' ');
subChar(ss, '\r',' ');
while (1)
    {
    if (!(w = nextWord(&s))) break;
    if(slNameInList(geneNames, w))
	/* could be optimized, but not expecting large lists for ring */
	{
	safef(emsg,sizeof(emsg), "Duplicate gene id (%s) found in list.",w);
	errMsg = cloneString(emsg);
	freez(&ss);
	return FALSE;
	}
    slNameAddHead(&geneNames, w);
    c++;
    }
slReverse(&geneNames);
if (c==0)
    {
    errMsg = "No genes in list. Please specify genes for ring.";
    return FALSE;
    }
freez(&ss);
return TRUE;
}

void updateGeneList()
/* update the cart list of genes */
{
char *sep = "";
struct slName *sn = geneNames;
struct dyString *geneList=newDyString(512);
while(sn)
    {
    dyStringPrintf(geneList,"%s%s",sep,sn->name);
    sep=" ";
    sn=sn->next;
    }
cartSetString(cart, "ring_geneList", geneList->string);
freeDyString(&geneList);
}

struct bdgpGeneInfo *bdgpGeneInfoLoadByQuery(struct sqlConnection *conn, char *query)
/* Load all interaction from table that satisfy the query given.
 * Where query is of the form 'select * from example where something=something'
 * or 'select example.* from example, anotherTable where example.something =
 * anotherTable.something'.
 * Dispose of this with bdgpGeneInfoFreeList(). */
{
struct bdgpGeneInfo *list = NULL, *el;
struct sqlResult *sr;
char **row;

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = bdgpGeneInfoLoad(row);
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);
return list;
}


void getGeneAliasesFromTable(char* table)
/* read all the gene aliases from database, return bdgpGeneInfo-list */
{
struct bdgpGeneInfo *result = NULL, *this=NULL;
struct dyString *query=newDyString(512);
char *sep = "where";
struct slName *sn = geneNames;
dyStringPrintf(query,"select * from %s",table);
while(sn)
    {
    char * nm = sn->name;
    if (startsWith("CG",nm))
	{
	dyStringPrintf(query," %s bdgpName='%s'",sep,nm);
    	sep = "or";
	}
    else if (!startsWith("FBgn",nm))
	{
	dyStringPrintf(query," %s symbol='%s'",sep,nm);
    	sep = "or";
	}
    sn = sn->next;
    }
//uglyf("<br>SQL=%s<br><br>\n",query->string);
struct sqlConnection* conn = hAllocConn();
result = bdgpGeneInfoLoadByQuery(conn, query->string);
for(this=result;this;this=this->next)
    {
    //uglyf("<br>adding %s=%s<br>\n",this->bdgpName,this->flyBaseId); // debug
    hashAdd(aliasHash,this->bdgpName,cloneString(this->flyBaseId));
    //uglyf("<br>adding %s=%s<br>\n",this->symbol,this->flyBaseId); // debug
    hashAdd(aliasHash,this->symbol,cloneString(this->flyBaseId));
    }
// Mysterious crashes if I free this list:
//bdgpGeneInfoFreeList(&result);
hFreeConn(&conn);
freeDyString(&query);
}

struct interaction *getGenesFromTable(char* table)
/* read all the gene interactions from database, returns interaction-list */
{
struct interaction *result = NULL;
struct dyString *query=newDyString(512);
char *sep = "where";
struct slName *sn = geneNames;
getGeneAliasesFromTable("bdgpGeneInfo"); /* load aliases into hash */
dyStringPrintf(query,"select * from %s",table);
while(sn)
    {
    struct hashEl *hel = NULL, *hf=NULL;
    char * nm = sn->name;
    char * alias = NULL;
    if (!startsWith("FBgn",nm))
	{
	if (hf = hashLookup(aliasHash,nm))
	    {
	    alias = sn->name;
	    //uglyf("<br>aliasing %s with %s<br>\n",nm,(char *)hf->val); // debug
	    nm = (char *)hf->val;
	    }
	}
    hel = hashStore(nodeHash,nm);
    if (!hel->val)                /* add geneList elements to the hash */
	{
	struct node *n;
	struct nodelist *nl;
	AllocVar(n);
	AllocVar(nl);
	n->name  = cloneString(nm);
	n->alias = cloneString(alias);
	n->ring = TRUE;          /* this is a special ring member */
	nl->node = n;
	slAddHead(&allNodes,nl); /* all nodes going in hash are added to all-nodes list */
	hel->val = n;
	}
    dyStringPrintf(query," %s fromX='%s' or toY='%s'",sep,nm,nm);
    sep = "or";
    sn = sn->next;
    }
//uglyf("<br>SQL=%s<br><br>\n",query->string);
struct sqlConnection* conn = hAllocConn();
result = interactionLoadByQuery(conn, query->string);
hFreeConn(&conn);
freeDyString(&query);
return result;
}


void getGeneList()
/* hgGeneRing - Gene Network Browser. */
{

/* not much point in this, though called old it's the same as current
   at this point.  It is not the old value from before the last submit.
char *oldGeneList = hashFindVal(oldVars, "ring_geneList");
*/

if (!sameString(errMsg,""))
    {
    printf("<CENTER>%s</CENTER>\n",errMsg);
    }

puts(
"<FORM ACTION=\"/cgi-bin/hgGeneRing\" NAME=\"mainForm\" METHOD=\"GET\">\n"
"<CENTER>"
"<TABLE CELLPADDING=1 style='background-color:#FFFEF3; border-style:none;'>\n"
"<TR><TD style='text-align:center; font-size:x-small;'>\n"
"Enter gene list for gene network ring.\n"
"</TD></TR></TABLE></CENTER>\n"
);

puts(
"<CENTER>"
"<table bgcolor='#CCCC99' border=0 CELLPADDING=1 CELLSPACING=0>\n"
"<tr>\n"
);

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
"</tr></table></CENTER>\n"
);
puts("</FORM>\n");

cartSetString(cart, "ring_action", "saveGeneList");
cartSetString(cart, "ring_errMsg", "");

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

boolean saveGeneList(boolean showAll)
/* Check for valid gene-list: parse and save.
   If list not ok, return to getGeneList page.
   Otherwise, show user's list and offer some links.
*/
{
struct interaction* intr;
struct nodelist *nl;

if (!getGenesAsList())
    {
    getGeneList();
    return FALSE;
    }


if (showAll)
    {
    puts(
    "<CENTER>"
    "<table bgcolor='#CCCC99' border=0 CELLPADDING=1 CELLSPACING=0>\n"
    "<tr><td>\n"
    );
    printf("geneList: %s<br>\n",
	geneList
    );
    puts(
    "</td></tr><tr><td>"
    "<a href=\"/cgi-bin/hgGeneRing?ring_action=getGeneList\">gene-list</a>\n"
    "</td></tr><tr><td>"
    "<a href=\"/cgi-bin/hgGeneRing?ring_action=drawScreen\">screen</a>\n"
    );
    puts(
    "</td></tr></table></CENTER>\n"
    );

    }


interactions = getGenesFromTable("intrP2P");  /* adds geneList elements to hash */

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



/* sum counts and if showAll, display our structure */
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



// TODO: add some real code for freeing these things properly
//   e.g. for xrays and nrays lists, free up the nodelist list itself,
//     but not the *node values themselves, which are allocated
//     in the main nodelist/hash.  If the nodelist is freed up
//     properly then hash freeing should be easy enough.
//     Do we use valgrind or some other util to verify mem-release?



return TRUE;
}


/* Count up elements in list that are outside ring. */
int slCountOutside(void *list)
{
struct slList *pt = (struct slList *)list;
int len = 0;

while (pt != NULL)
    {
    struct node *n = ((struct nodelist *)pt)->node;
    if ((!n->ring) && (n->ringRank==1))
	len += 1;
    pt = pt->next;
    }
return len;
}

#define NODE_SIZE 10
void addMap(struct nodelist *nl)
{
struct node *node = nl->node;
int x=node->xpos;
int y=node->ypos;
int r=NODE_SIZE;
char *name = node->name;
if (node->alias)
    {
    name = node->alias;
    }
printf(
 "<AREA SHAPE=CIRCLE COORDS=\"%d,%d,%d\""
 " HREF=\"/cgi-bin/hgGeneRing?ring_action=geneFrame&ring_gene=%s&ring_position=%s\""
 " ALT=\"%s\""
 " TITLE=\"%s\">",
 x,y,r,
 name,
 node->name,
 name,
 name
 );
}

void drawScreen()
/*
 Draw Gene Ring using given genes and interactions
 and display it in a page as a .gif
*/
{

#define SCREEN_SIZE 800
#define RING_SIZE 200
#define INRING_SIZE 140
#define OUTRING_SIZE 340


struct tempName filename;
int width,height;
char *mapName="ring_map";

ZeroVar(&filename);
makeTempName(&filename, "hgGeneRing_screen", ".gif");

printf("<MAP Name=%s>\n", mapName);

struct memGfx *mg = mgNew(SCREEN_SIZE,SCREEN_SIZE);
int i;
double angle=0;
int xCen = SCREEN_SIZE/2, yCen = SCREEN_SIZE/2;
int x=0,y=0;
struct nodelist *nl, *saveNl;

/* draw the main RING */
mgCircle(mg, xCen, yCen, SCREEN_SIZE/4, MG_BLACK, FALSE);

/* draw the RING genes and assign position */
angle=(M_PI*2)/ringCount;
nl=allNodes;
for(i=0;i<ringCount;i++)
    {
    x = xCen + (int) RING_SIZE*cos(i*angle);
    y = yCen + (int) RING_SIZE*sin(i*angle);
    nl->node->xpos = x;
    nl->node->ypos = y;
    mgCircle(mg, x, y, NODE_SIZE, MG_BLACK, FALSE);
    addMap(nl);
    nl=nl->next;
    }

/* draw the Inside genes, assign position, and draw rays */
angle=(M_PI*2)/insideCount;
for(i=0;i<insideCount;i++)
    {
    x = xCen + (int) INRING_SIZE*cos((i+.5)*angle);
    y = yCen + (int) INRING_SIZE*sin((i+.5)*angle);
    nl->node->xpos = x;
    nl->node->ypos = y;
    mgCircle(mg, x, y, NODE_SIZE, MG_BLACK, FALSE);
    addMap(nl);

    /* draw connections to ring */
    { /*local*/
    struct nodelist *l;
    for(l=nl->node->xrays;l;l=l->next)
	{ /* blue = inward = to-ring */
	mgDrawLine(mg, x, y, l->node->xpos, l->node->ypos, MG_BLUE);
	}
    for(l=nl->node->nrays;l;l=l->next)
	{ /* red = outward = from-ring */
	mgDrawLine(mg, x, y, l->node->xpos, l->node->ypos, MG_RED);
	}
    }

    nl=nl->next;
    }


/* draw the Outside genes, assign position, draw rays
    do 1 ring member at a time.
*/
angle=(M_PI*2)/ringCount;
nl=allNodes;
for(i=0;i<ringCount;i++)
    {

    double subAngle=0;
    int numOutside=0, doneOutside=0;
    numOutside += slCountOutside(nl->node->nrays);
    numOutside += slCountOutside(nl->node->xrays);
    subAngle = angle / (numOutside+1);  /* add 1 to give a little space between ring groups */

    /* draw connections to ring */
    { /*local*/
    struct nodelist *l;
    int j = 0;
    struct nodelist *rays;
    Color color;
    for (j=0;j<2;j++)
	{
	if (j==0)
	    {
    	    color = MG_BLUE;
    	    rays = nl->node->xrays;
	    }
	else
	    {
    	    color = MG_RED;
    	    rays = nl->node->nrays;
	    }
	for(l=rays;l;l=l->next)
	    {
	    if (l->node->ring)
		{ /* this ring-ring line gets draw twice but that's ok */
		x = l->node->xpos;
		y = l->node->ypos;
		mgDrawLine(mg, x, y, nl->node->xpos, nl->node->ypos, MG_BLACK);
		}
	    else if (l->node->ringRank==1)
		{
		doneOutside++;
		x = xCen + (int) OUTRING_SIZE*cos((i-.5)*angle+(doneOutside*subAngle));
		y = yCen + (int) OUTRING_SIZE*sin((i-.5)*angle+(doneOutside*subAngle));
		l->node->xpos = x;
		l->node->ypos = y;
		mgCircle(mg, x, y, NODE_SIZE, MG_BLACK, FALSE);
		addMap(l);
		mgDrawLine(mg, x, y, nl->node->xpos, nl->node->ypos, color);
		}
	    }
	}
    }

    nl=nl->next;
    }


printf("</MAP>\n");



mgSaveGif(mg, filename.forCgi, FALSE);

width=height=SCREEN_SIZE;
printf("<CENTER><TABLE BORDER=0 CELLPADDING=0>");
printf("<TR><TD HEIGHT=5></TD></TR>");
printf("<TR><TD><IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s >",
       filename.forHtml, width, height, mapName);
printf("</TD></TR>");
printf("<TR><TD HEIGHT=5></TD></TR>");
printf(
    "<TR><TD>"
    "<a href = \"/cgi-bin/hgGeneRing?ring_action=saveGeneList\">back</a> "
    "<a href = \"/cgi-bin/hgGeneRing?ring_action=getGeneList\">gene-list</a> "
    "<a href = \"/cgi-bin/hgGeneRing?ring_action=drawScreen\">refresh</a> "
    "</TD></TR>");
printf("<TR><TD HEIGHT=5></TD></TR></TABLE></CENTER>");

}

char *unAlias(char *gene)
/* deal with alias mapping for gene names, return FBgn */
{
struct hashEl *hf=NULL;
if (!startsWith("FBgn",gene))
    {
    if (hf=hashLookup(aliasHash,gene))
	{
	gene=(char *)hf->val;
	}
    }
return gene;
}

void drawDetails()
{
char *gene = cartUsualString(cart, "ring_gene", "");
char *targ = unAlias(gene);
struct hashEl *hel = NULL;
struct node *n;
hel = hashLookup(nodeHash,targ);
if (hel)
    {
    n = hel->val;
    printf(
	"Details page for gene %s "
	"<A HREF=\"/cgi-bin/hgGeneRing?ring_action=%s&ring_gene=%s\" target=\"_parent\" >"
	"%s Ring</A> "
	" (rank=%d) "
	"<A HREF=\"/cgi-bin/hgGeneRing?ring_action=drawScreen\" target=\"_parent\" > return </A>"
	"\n",
	gene,
	n->ring?"removeFromRing":"addToRing",
	gene,
	n->ring?"Remove From ":"Add To ",
	n->ringRank
	);
    }
else
    {
    printf("Unknown gene %s",gene);
    }
}


void hgGeneRing()
/* hgGeneRing - Gene Network Browser. */
{
char *action = cartUsualString(cart, "ring_action", "");

if (sameWord(action,""))
    {
    action = "getGeneList";
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
    if (saveGeneList(FALSE))
    	drawScreen();
    }
else if (sameWord(action,"drawDetails"))
    {
    if (saveGeneList(FALSE))
    	drawDetails();
    }
else if (sameWord(action,"addToRing"))
    {
    char *gene = cartUsualString(cart, "ring_gene", "");
    getGenesAsList();
    slNameAddTail(&geneNames, gene);
    updateGeneList();
    /* in lieue of actually just redirecting the browser */
    cartSaveSession(cart);
    geneList = cloneString(cartUsualString(cart, "ring_geneList", ""));
    slFreeList(&geneNames); /* just reset everything */
    if (saveGeneList(FALSE))
	drawScreen();
    }
else if (sameWord(action,"removeFromRing"))
    {
    char *gene = cartUsualString(cart, "ring_gene", "");
    struct slName **psn = &geneNames;
    getGenesAsList();
    while(*psn)
	{
	if (sameString((*psn)->name,gene))
	    {
	    *psn = (*psn)->next;
	    break;
	    }
	psn=&((*psn)->next);
	}
    updateGeneList();
    /* in lieue of actually just redirecting the browser */
    cartSaveSession(cart);
    geneList = cloneString(cartUsualString(cart, "ring_geneList", ""));
    slFreeList(&geneNames); /* just reset everything */
    if (saveGeneList(FALSE))
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
char *action = cgiUsualString("ring_action", "");
cart = theCart;

getDbGenomeClade(cart, &db, &organism, &clade, oldVars);
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

if (sameWord(action,"geneFrame"))
    { /* special handling for frameset - can't use cartWebStart etc. */
    char *gene = cartUsualString(cart, "ring_gene", "");
    char *position = cartUsualString(cart, "ring_position", "");
    printf(
	"<frameset rows = \"18%%, *\">"
    	"  <frame src =\"/cgi-bin/hgGeneRing?ring_action=drawDetails&ring_gene=%s\" />"
	"  <frame src =\"/cgi-bin/hgTracks?position=%s&db=%s\" />"
    	"</frameset>",
	gene,
	position,
	db
	);
    }
else
    {
    cartWebStart(theCart, database, "%s Gene Network Browser \n", organism);
    geneList = cloneString(cartUsualString(cart, "ring_geneList", ""));
    errMsg = cartUsualString(cart, "ring_errMsg", "");  /* any error from last time */

    hgGeneRing();

    cartSaveSession(cart);
    cartWebEnd();
    }
freez(&geneList);
}

char *excludeVars[] = {NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(10);
cgiSpoof(&argc, argv);

nodeHash = newHash(8);
aliasHash = newHash(8);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
freeHashAndVals(&nodeHash);
return 0;
}

