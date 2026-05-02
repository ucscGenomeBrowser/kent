/* Handle details pages for myVariants tracks */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "hgc.h"
#include "myVariants.h"
#include "myVariantsShare.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "hgMaf.h"
#include "hui.h"
#include "hCommon.h"
#include "wikiLink.h"
#include "jsHelper.h"
#include "hgConfig.h"
#include "jsonWrite.h"
#include "htmshell.h"

void doMyVariantsDetails(struct customTrack *ct, char *itemIdString)
/* Show details of a myVariants item. */
{
jsIncludeFile("hgc.js",NULL);
char *idString = cloneString(itemIdString);
char *trackName = ct->tdb->track;

/* Detect shared track and resolve table/permissions */
boolean isShared = startsWith("myVariants_shared_", trackName);
char *dataOwner = NULL;     /* user whose table holds the data */
int permission = MYVAR_PERM_READONLY;
if (isShared)
    {
    char *token = trackName + strlen("myVariants_shared_");
    char cartVar[256];
    safef(cartVar, sizeof(cartVar), MYVAR_SHARED_CART_PREFIX "%s", token);
    char *cartVal = cartOptionalString(cart, cartVar);
    if (isEmpty(cartVal))
        {
        printf("Share information not found.\n");
        return;
        }
    char *project = NULL, *db = NULL;
    if (!myVariantsParseShareCartValue(cartVal, &dataOwner, &project, &db, &permission, NULL))
        {
        printf("Invalid share data.\n");
        return;
        }
    freeMem(project);
    freeMem(db);
    }
else
    dataOwner = cloneString(getUserName());

/* Anon users never edit shared items, even when the share is read-write. */
boolean canEdit = !isShared || (permission == MYVAR_PERM_READWRITE && getUserName() != NULL);

char *tableName = myVariantsGetDbTable(dataOwner);
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
char query[512];
sqlSafef(query, sizeof(query), "select * from %s where concat(id,' ',name)='%s'",
    tableName, idString);
struct sqlResult *sr = sqlGetResult(conn, query);

char **row;
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct myVariants *item = myVariantsLoad(row);
    sqlFreeResult(&sr);  /* Free early so conn is available for custom field queries */

    /* Show shared banner */
    if (isShared)
        {
        if (canEdit)
            printf("<div style='padding:6px; margin-bottom:8px; background:#e8f5e9; "
                "border:1px solid #a5d6a7; border-radius:4px'>"
                "<B>Shared from %s</B></div>\n", dataOwner);
        else
            printf("<div style='padding:6px; margin-bottom:8px; background:#e3f2fd; "
                "border:1px solid #90caf9; border-radius:4px'>"
                "<B>Shared from %s (read-only)</B></div>\n", dataOwner);
        }

    if (canEdit)
        {
        printf("<FORM ACTION=\"%s\" METHOD=\"POST\">\n\n", hgTracksName());
        cartSaveSession(cart);

        /* Save away ID string in hidden var. */
        char varName[128];
        char idStr[128];
        safef(varName, sizeof(varName), "%s_%s", trackName, "id");
        safef(idStr, sizeof(idStr), "%d", item->id);
        cgiMakeHiddenVar(varName, idStr);

        /* Put up editable label. */
        safef(varName, sizeof(varName), "%s_%s", trackName, "name");
        printf("<B>Label:</B> ");
        cgiMakeTextVar(varName, item->name, 17);
        printInfoIcon("A short label for this variant, displayed in the browser.");
        printf("<BR>\n");

        /* Put up editable description. */
        safef(varName, sizeof(varName), "%s_%s", trackName, "description");
        printf("<B>Description:</B> ");
        printInfoIcon("Longer notes or comments about this variant. Displayed on this details page.");
        printf("<BR>\n");
        cgiMakeTextArea(varName, item->description, 8, 80);
        printf("<BR>\n");

        /* Non-editable chromosome. */
        printf("<B>Chromosome:</B> %s<BR>\n", item->chrom);

        /* Editable start and end. */
        int chromSize = hChromSize(database, item->chrom);
        char chromSizeString[16];
        safef(chromSizeString, sizeof(chromSizeString), "%d", chromSize);
        printf("<B>Start:</B> ");
        safef(varName, sizeof(varName), "%s_%s", trackName, "chromStart");
        cgiMakeIntVarInRange(varName, item->chromStart+1, NULL, 80, "1", chromSizeString);
        printInfoIcon("1-based start position on the chromosome.");
        printf("<BR>\n");
        printf("<B>End:</B> ");
        safef(varName, sizeof(varName), "%s_%s", trackName, "chromEnd");
        cgiMakeIntVarInRange(varName, item->chromEnd, NULL, 80, "1", chromSizeString);
        printInfoIcon("1-based end position on the chromosome (inclusive).");
        printf("<BR>\n");

        /* Edit the color */
        safef(varName, sizeof(varName), "%s_%s", trackName, "itemRgb");
        char colorHex[8];
        safef(colorHex, sizeof(colorHex), "#%06X", item->itemRgb);
        cgiMakeColorVarWithLabel(varName, "Color", colorHex, TRUE);
        printf("<br>");

        /* Edit ref/alt */
        safef(varName, sizeof(varName), "%s_%s", trackName, "ref");
        printf("<B>Ref:</B> ");
        cgiMakeTextVar(varName, item->ref, 17);
        printInfoIcon("Reference allele sequence at this position.");
        printf("<BR>\n");
        safef(varName, sizeof(varName), "%s_%s", trackName, "alt");
        printf("<B>Alt:</B> ");
        cgiMakeTextVar(varName, item->alt, 17);
        printInfoIcon("Alternate (variant) allele sequence.");
        printf("<BR>\n");

        /* Project: locked for shared tracks, editable for own track */
        printf("<B>Project:</B> ");
        if (isShared)
            {
            printf("%s", isNotEmpty(item->project) ? item->project : "(none)");
            printf("<BR>\n");
            }
        else
            {
            safef(varName, sizeof(varName), "%s_%s", trackName, "project");
            struct slName *projects = myVariantsGetProjects(dataOwner);
            if (projects)
                {
                char selectName[128];
                safef(selectName, sizeof(selectName), "%s_projectSelect", trackName);
                printf("<select id='%s'>", selectName);
                printf("<option value=''>%s</option>", "(none)");
                struct slName *proj;
                boolean currentFound = FALSE;
                for (proj = projects; proj != NULL; proj = proj->next)
                    {
                    char *selected = "";
                    if (sameString(proj->name, item->project))
                        {
                        selected = " selected";
                        currentFound = TRUE;
                        }
                    printf("<option value='%s'%s>%s</option>", proj->name, selected, proj->name);
                    }
                if (!currentFound && isNotEmpty(item->project))
                    printf("<option value='%s' selected>%s</option>", item->project, item->project);
                printf("<option value='__new__'>Add new...</option>");
                printf("</select> ");
                printf("<input type='text' name='%s' id='%s' value='%s' style='display:none'"
                    " placeholder='Enter new project'>", varName, varName, item->project);
                slFreeList(&projects);
                }
            else
                cgiMakeTextVar(varName, item->project, 40);
            printInfoIcon("Group variants by project.");
            printf("<BR>\n");
            }

        /* Mouseover */
        safef(varName, sizeof(varName), "%s_%s", trackName, "mouseover");
        printf("<B>Mouseover:</B> ");
        cgiMakeTextVar(varName, item->mouseover, 60);
        printInfoIcon("Short text shown when hovering over this item.");
        printf("<BR>\n");

        /* Custom fields */
            {
            struct slName *customCols = myVariantsGetCustomFields(dataOwner);
            if (customCols)
                {
                struct dyString *cfQuery = dyStringNew(256);
                struct slName *col;
                sqlDyStringPrintf(cfQuery, "SELECT ");
                boolean first = TRUE;
                for (col = customCols; col != NULL; col = col->next)
                    {
                    if (!first)
                        sqlDyStringPrintf(cfQuery, ", ");
                    sqlDyStringPrintIdList(cfQuery, col->name);
                    first = FALSE;
                    }
                sqlDyStringPrintf(cfQuery, " FROM %s WHERE id=%d", tableName, item->id);
                struct sqlResult *cfSr = sqlGetResult(conn, dyStringContents(cfQuery));
                char **cfRow = sqlNextRow(cfSr);
                if (cfRow)
                    {
                    int i = 0;
                    for (col = customCols; col != NULL; col = col->next, i++)
                        {
                        safef(varName, sizeof(varName), "%s_%s", trackName, col->name);
                        printf("<B>%s:</B> ", col->name);
                        cgiMakeTextVar(varName, cfRow[i] ? cfRow[i] : "", 40);
                        printf("<BR>\n");
                        }
                    }
                sqlFreeResult(&cfSr);
                dyStringFree(&cfQuery);
                slFreeList(&customCols);
                }
            }

        /* Buttons: Update and Cancel always; Delete only for own track */
        cgiMakeButton("submit", "Update");
        printf(" ");
        if (!isShared)
            {
            safef(varName, sizeof(varName), "%s_%s", trackName, "delete");
            cgiMakeButton(varName, "Delete");
            printf(" ");
            }
        safef(varName, sizeof(varName), "%s_%s", trackName, "cancel");
        cgiMakeButton(varName, "Cancel");
        printf("</FORM>\n");
        }
    else
        {
        /* Read-only display for shared items without write permission.
         * htmlPrintf escapes %s by default; that prevents stored XSS via
         * owner-controlled fields. */
        htmlPrintf("<B>Label:</B> %s<BR>\n", item->name);
        htmlPrintf("<B>Description:</B><BR>\n%s<BR>\n", item->description);
        htmlPrintf("<B>Chromosome:</B> %s<BR>\n", item->chrom);
        printf("<B>Start:</B> %d<BR>\n", item->chromStart + 1);
        printf("<B>End:</B> %d<BR>\n", item->chromEnd);
        char colorHex[8];
        safef(colorHex, sizeof(colorHex), "#%06X", item->itemRgb);
        printf("<B>Color:</B> <span style='background:%s; padding:2px 12px'>&nbsp;</span> %s<BR>\n",
            colorHex, colorHex);
        if (isNotEmpty(item->ref))
            htmlPrintf("<B>Ref:</B> %s<BR>\n", item->ref);
        if (isNotEmpty(item->alt))
            htmlPrintf("<B>Alt:</B> %s<BR>\n", item->alt);
        if (isNotEmpty(item->project))
            htmlPrintf("<B>Project:</B> %s<BR>\n", item->project);
        if (isNotEmpty(item->mouseover))
            htmlPrintf("<B>Mouseover:</B> %s<BR>\n", item->mouseover);

        /* Custom fields read-only */
            {
            struct slName *customCols = myVariantsGetCustomFields(dataOwner);
            if (customCols)
                {
                struct dyString *cfQuery = dyStringNew(256);
                struct slName *col;
                sqlDyStringPrintf(cfQuery, "SELECT ");
                boolean first = TRUE;
                for (col = customCols; col != NULL; col = col->next)
                    {
                    if (!first)
                        sqlDyStringPrintf(cfQuery, ", ");
                    sqlDyStringPrintIdList(cfQuery, col->name);
                    first = FALSE;
                    }
                sqlDyStringPrintf(cfQuery, " FROM %s WHERE id=%d", tableName, item->id);
                struct sqlResult *cfSr = sqlGetResult(conn, dyStringContents(cfQuery));
                char **cfRow = sqlNextRow(cfSr);
                if (cfRow)
                    {
                    int i = 0;
                    for (col = customCols; col != NULL; col = col->next, i++)
                        {
                        if (cfRow[i] && cfRow[i][0])
                            htmlPrintf("<B>%s:</B> %s<BR>\n", col->name, cfRow[i]);
                        }
                    }
                sqlFreeResult(&cfSr);
                dyStringFree(&cfQuery);
                slFreeList(&customCols);
                }
            }
        }

    printf("<B>id:</B> %d<BR>\n", item->id);

    /* Overlaps section: only emit if hg.conf names overlap tracks for this assembly.
     * Format: myVariantsOverlapTracks.<db> = track1,track2,...  */
    char overlapKey[256];
    safef(overlapKey, sizeof(overlapKey), "myVariantsOverlapTracks.%s", database);
    char *overlapList = cfgOption(overlapKey);
    if (isNotEmpty(overlapList))
        {
        struct jsonWrite *jw = jsonWriteNew();
        jsonWriteListStart(jw, NULL);
        struct slName *trackNames = slNameListFromComma(overlapList);
        struct slName *t;
        for (t = trackNames; t != NULL; t = t->next)
            jsonWriteString(jw, NULL, t->name);
        jsonWriteListEnd(jw);
        jsInline("var doItemOverlaps = true;\n");
        jsInlineF("var overlapTracks = %s;\n", jw->dy->string);
        printf("<div id='itemOverlaps' style=\"display:none\"></div>\n");
        slFreeList(&trackNames);
        jsonWriteFree(&jw);
        }

    printPosOnChrom(item->chrom, item->chromStart, item->chromEnd, NULL, TRUE, NULL);
    }
else
    sqlFreeResult(&sr);

freeMem(dataOwner);
hFreeConn(&conn);
}

