/* hgEncodeDataVersions - print ENCODE tracks with data version */

#include "common.h"
#include "cheapcgi.h"
#include "hui.h"
#include "trackDb.h"
#include "grp.h"
#include "hdb.h"
#include "hCommon.h"
#include "cart.h"
#include "web.h"

static char const rcsid[] = "$Id: hgEncodeDataVersions.c,v 1.5 2008/09/03 19:18:27 markd Exp $";

/* Global variables */
struct cart *cart;
struct hash *oldCart = NULL;

struct trackRef
/* reference to trackDb, for constructing lists on top of lists */
{
    struct trackRef *next;        /* Next in list */
    struct trackDb *track;        /* Underlying trackDb */
};

struct group
/* group with list of its tracks */
{
    struct group *next;  /* Next in singly linked list. */
    char *name;	         /* Group name.  Connects with trackDb.grp */
    char *label;	 /* Label to display to user */
    struct trackRef *tracks;
};

int cmpGroupPri(const void *va, const void *vb)
/* Compare groups based on priority */
{
const struct grp *a = *((struct grp **)va);
const struct grp *b = *((struct grp **)vb);
return (a->priority - b->priority);
}

int cmpTrackPri(const void *va, const void *vb)
/* Compare tracks based on priority */
{
const struct trackRef *a = *((struct trackRef **)va);
const struct trackRef *b = *((struct trackRef **)vb);
return (a->track->priority - b->track->priority);
}

struct group *groupTracks(char *db, struct trackDb *tracks)
/* Make up groups and assign tracks to groups. */
{
struct trackDb *track;
struct trackRef *tr;
struct group *group, *groups = NULL;
struct grp *grp;
struct grp *grps = hLoadGrps(db);
struct hash *groupHash = newHash(8);

/* Sort groups by priority */
slSort(&grps, cmpGroupPri);

/* Create hash and list of groups */
for (grp = grps; grp != NULL; grp = grp->next)
    {
    AllocVar(group);
    group->name = cloneString(grp->name);
    group->label = cloneString(grp->label);
    slAddTail(&groups, group);
    hashAdd(groupHash, grp->name, group);
    }

/* Add tracks to group */
for (track = tracks; track != NULL; track = track->next)
    {
    AllocVar(tr);
    tr->track = track;
    group = hashFindVal(groupHash, track->grp);
    slAddHead(&group->tracks, tr);
    }

/* order tracks within groups by priority */
for (group = groups; group != NULL; group = group->next)
    slSort(&group->tracks, cmpTrackPri);
hashFree(&groupHash);
return groups;
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
char *db = NULL;
char *ignored;
struct trackDb *tracks;
struct trackRef *tr;
struct group *group, *groups = NULL;

cart = theCart;
cartWebStart(cart, NULL, "ENCODE Track Data Versions (%s)", db);
getDbAndGenome(cart, &db, &ignored, NULL);
tracks = hTrackDb(db, NULL);
groups = groupTracks(db, tracks);
for (group = groups; group != NULL; group = group->next)
    {
    if (group->tracks == NULL || !startsWith("encode", group->name))
        continue;
    hTableStart();
    puts("<TR>");
    puts("<TH align=LEFT colspan=3 BGCOLOR=#536ED3>");
    printf("<B>&nbsp;%s</B> ", wrapWhiteFont(group->label));
    printf("&nbsp;&nbsp;&nbsp; ");
    puts("</TH>\n");
    puts("</TR>");
    for (tr = group->tracks; tr != NULL; tr = tr->next)
        {
        char *version = trackDbSetting(tr->track, "dataVersion");
        if (version)
            stripString(version, "ENCODE ");
	puts("<TR><TD>");
	printf(" %s", tr->track->shortLabel);
	puts("</TD><TD>");
	printf(" %s", version ? version : "&nbsp;");
	puts("</TD><TD>");
	printf(" %s", tr->track->longLabel);
	puts("</TD></TR>");
	}
    /*
        printf("\t%s\t%s\t%s\n", version, 
                tr->track->shortLabel, tr->track->longLabel);
                */
    hTableEnd();
    puts("<BR>");
    }
cartWebEnd();
}

int main(int argc, char *argv[])
/* Process command line */
{
oldCart = hashNew(8);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), NULL, oldCart);
return 0;
}

