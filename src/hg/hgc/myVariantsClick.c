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
#include "web.h"
#include "hgConfig.h"
#include "jsonWrite.h"
#include "htmshell.h"

void doMyVariantsDetails(struct customTrack *ct, char *itemIdString)
/* Show details of a myVariants item. */
{
jsIncludeFile("hgc.js",NULL);
char *idString = cloneString(itemIdString);
char *trackName = ct->tdb->track;

/* Detect shared track and resolve table/permissions via hgcentral so that
 * revoked or downgraded shares no longer return owner data. */
boolean isShared = isMyVariantsSharedTrack(trackName);
char *dataOwner = NULL;     /* user whose table holds the data */
char *scopeProject = NULL;  /* live share's project, or NULL for own track */
char *scopeDb = NULL;       /* live share's db, or NULL for own track */
int permission = MYVAR_PERM_READONLY;
if (isShared)
    {
    struct myVariantsShare *share = myVariantsResolveSharedTrack(trackName, cart);
    if (share == NULL)
        {
        printf("Share is no longer available.\n");
        return;
        }
    /* Shared tracks are per-assembly; reject details requests from other dbs. */
    if (!sameString(share->db, database))
        {
        printf("This share is for a different assembly.\n");
        myVariantsShareFree(&share);
        return;
        }
    dataOwner = cloneString(share->ownerUser);
    scopeProject = cloneString(share->project);
    scopeDb = cloneString(share->db);
    permission = share->permission;
    myVariantsShareFree(&share);
    }
else
    dataOwner = cloneString(getUserName());

/* Anon users never edit shared items, even when the share is read-write. */
boolean canEdit = !isShared || (permission == MYVAR_PERM_READWRITE && getUserName() != NULL);

char *tableName = myVariantsGetDbTable(dataOwner);
/* idString is "<id> <name>": parse id, query by id, verify name matches. */
char *idStrCopy = cloneString(idString);
char *expectedName = strchr(idStrCopy, ' ');
if (expectedName == NULL)
    {
    printf("Invalid item identifier.\n");
    freeMem(idStrCopy);
    return;
    }
*expectedName++ = '\0';
if (!isAllDigits(idStrCopy))
    {
    printf("Invalid item identifier.\n");
    freeMem(idStrCopy);
    return;
    }
unsigned itemId = sqlUnsigned(idStrCopy);

struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
struct dyString *query = sqlDyStringCreate(
    "select * from %s where id=%u", tableName, itemId);
if (isNotEmpty(scopeDb))
    sqlDyStringPrintf(query, " and db='%s'", scopeDb);
if (isNotEmpty(scopeProject) && !sameString(scopeProject, "*"))
    sqlDyStringPrintf(query, " and project='%s'", scopeProject);
struct sqlResult *sr = sqlGetResult(conn, query->string);
dyStringFree(&query);

struct myVariants *item = NULL;
char **row = sqlNextRow(sr);
if (row != NULL)
    {
    item = myVariantsLoad(row);
    if (!sameOk(item->name, expectedName))
        myVariantsFree(&item);
    }
sqlFreeResult(&sr);
freeMem(idStrCopy);

if (item != NULL)
    {
    /* Show shared banner */
    if (isShared)
        {
        if (canEdit)
            printf("<div style='padding:6px; margin-bottom:8px; background:#e8f5e9; "
                "border:1px solid #a5d6a7; border-radius:4px'>"
                "<B>Shared from %s</B></div>\n", htmlEncode(dataOwner));
        else
            printf("<div style='padding:6px; margin-bottom:8px; background:#e3f2fd; "
                "border:1px solid #90caf9; border-radius:4px'>"
                "<B>Shared from %s (read-only)</B></div>\n", htmlEncode(dataOwner));
        }

    if (canEdit)
        {
        webIncludeResourceFile("spectrum.min.css");
        jsIncludeFile("spectrum.min.js", NULL);
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
        printInfoIcon("A short label for this annotation, displayed in the browser.");
        printf("<BR>\n");

        /* Put up editable description. */
        safef(varName, sizeof(varName), "%s_%s", trackName, "description");
        printf("<B>Description:</B> ");
        printInfoIcon("Longer notes or comments about this annotation. Displayed on this details page.");
        printf("<BR>\n");
        cgiMakeTextArea(varName, item->description, 8, 80);
        printf("<BR>\n");

        /* Non-editable chromosome. */
        htmlPrintf("<B>Chromosome:</B> %s<BR>\n", item->chrom);

        /* Editable start and end. */
        int chromSize = hChromSize(database, item->chrom);
        char chromSizeString[16];
        safef(chromSizeString, sizeof(chromSizeString), "%d", chromSize);
        printf("<B>Start:</B> ");
        safef(varName, sizeof(varName), "%s_%s", trackName, "chromStart");
        cgiMakeIntVarInRange(varName, item->chromStart, NULL, 80, "0", chromSizeString);
        printInfoIcon("0-based start position on the chromosome.");
        printf("<BR>\n");
        printf("<B>End:</B> ");
        safef(varName, sizeof(varName), "%s_%s", trackName, "chromEnd");
        cgiMakeIntVarInRange(varName, item->chromEnd, NULL, 80, "1", chromSizeString);
        printInfoIcon("0-based half-open end position on the chromosome.");
        printf("<BR>\n");

        /* Blocks (BED12). Hidden inputs are kept in sync by the widget;
         * updateBlocksFields in hgTracks reads them on submit. */
        char vBC[128], vBS[128], vBT[128], vCS[128], vCE[128];
        safef(vBC, sizeof(vBC), "%s_blockCount", trackName);
        safef(vBS, sizeof(vBS), "%s_blockSizes", trackName);
        safef(vBT, sizeof(vBT), "%s_chromStarts", trackName);
        safef(vCS, sizeof(vCS), "%s_chromStart", trackName);
        safef(vCE, sizeof(vCE), "%s_chromEnd", trackName);
        struct dyString *sizesCsv = dyStringNew(64);
        struct dyString *startsCsv = dyStringNew(64);
        int bi;
        for (bi = 0; bi < item->blockCount; bi++)
            {
            if (bi > 0)
                {
                dyStringAppendC(sizesCsv, ',');
                dyStringAppendC(startsCsv, ',');
                }
            dyStringPrintf(sizesCsv, "%d", item->blockSizes[bi]);
            dyStringPrintf(startsCsv, "%d", item->chromStarts[bi]);
            }
        char countStr[32];
        safef(countStr, sizeof(countStr), "%u", item->blockCount);
        printf("<B>Blocks:</B> ");
        printInfoIcon("BED12-style blocks. Leave empty to store a single full-span block.");
        printf("<BR>\n");
        cgiMakeHiddenVarWithIdExtra(vBC, vBC, countStr, NULL);
        cgiMakeHiddenVarWithIdExtra(vBS, vBS, dyStringContents(sizesCsv), NULL);
        cgiMakeHiddenVarWithIdExtra(vBT, vBT, dyStringContents(startsCsv), NULL);
        printf("<div id=\"myVariantsBlocksUi\" style=\"margin:4px 0 12px 0;\"></div>\n");
        jsIncludeFile("myVariantsBlocks.js", NULL);
        jsInlineF(
            "$(function(){\n"
            "  if (typeof myVariantsBlocks === 'undefined') { return; }\n"
            "  var sizesEl = document.getElementById('%s');\n"
            "  var startsEl = document.getElementById('%s');\n"
            "  var sizes = sizesEl.value ? sizesEl.value.split(',').map(Number) : [];\n"
            "  var starts = startsEl.value ? startsEl.value.split(',').map(Number) : [];\n"
            "  myVariantsBlocks.mount('myVariantsBlocksUi', {\n"
            "    initialSizes: sizes,\n"
            "    initialStarts: starts,\n"
            "    getStart: function(){ return parseInt(document.getElementById('%s').value, 10); },\n"
            "    getEnd:   function(){ return parseInt(document.getElementById('%s').value, 10); },\n"
            "    hiddenCountInput:  document.getElementById('%s'),\n"
            "    hiddenSizesInput:  sizesEl,\n"
            "    hiddenStartsInput: startsEl\n"
            "  });\n"
            "});\n",
            vBS, vBT, vCS, vCE, vBC);
        dyStringFree(&sizesCsv);
        dyStringFree(&startsCsv);

        /* Edit the color */
        safef(varName, sizeof(varName), "%s_%s", trackName, "itemRgb");
        char colorHex[8];
        safef(colorHex, sizeof(colorHex), "#%06X", item->itemRgb);
        hPrintf("<label for=\"%s\"><b>Color:</b></label> ", varName);
        hPrintf("<input type=\"text\" name=\"%s\" id=\"%s\" value=\"%s\">\n",
            varName, varName, colorHex);
        jsInlineF(
            "$(function() {"
                "$(document.getElementById('%s')).spectrum({"
                    "preferredFormat: 'hex',"
                    "showInput: true,"
                    "showPalette: true,"
                    "hideAfterPaletteSelect: true"
                "});"
            "});\n",
            varName);
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
            htmlPrintf("%s<BR>\n", isNotEmpty(item->project) ? item->project : "(none)");
            }
        else
            {
            safef(varName, sizeof(varName), "%s_%s", trackName, "project");
            struct slName *projects = myVariantsGetProjects(dataOwner);
            if (projects)
                {
                char selectName[128];
                safef(selectName, sizeof(selectName), "%s_projectSelect", trackName);
                htmlPrintf("<select id='%s|attr|'>", selectName);
                printf("<option value=''>(none)</option>");
                struct slName *proj;
                boolean currentFound = FALSE;
                for (proj = projects; proj != NULL; proj = proj->next)
                    {
                    boolean isCurrent = sameString(proj->name, item->project);
                    if (isCurrent)
                        currentFound = TRUE;
                    htmlPrintf("<option value='%s|attr|'%s|none|>%s</option>",
                        proj->name, isCurrent ? " selected" : "", proj->name);
                    }
                if (!currentFound && isNotEmpty(item->project))
                    htmlPrintf("<option value='%s|attr|' selected>%s</option>",
                        item->project, item->project);
                printf("<option value='__new__'>Add new...</option>");
                printf("</select> ");
                htmlPrintf("<input type='text' name='%s|attr|' id='%s|attr|' value='%s|attr|'"
                    " style='display:none' placeholder='Enter new project'>",
                    varName, varName, item->project);
                slFreeList(&projects);
                }
            else
                cgiMakeTextVar(varName, item->project, 40);
            printInfoIcon("Group annotations by project.");
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
        printf("<B>Start:</B> %d<BR>\n", item->chromStart);
        printf("<B>End:</B> %d<BR>\n", item->chromEnd);
        if (item->blockCount > 1)
            {
            printf("<B>Blocks:</B> ");
            int roBi;
            for (roBi = 0; roBi < item->blockCount; roBi++)
                {
                if (roBi > 0)
                    printf(", ");
                int relStart = item->chromStarts[roBi];
                int relEnd = relStart + item->blockSizes[roBi];
                printf("%d-%d (%dbp)", relStart, relEnd, item->blockSizes[roBi]);
                }
            printf("<BR>\n");
            }
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

freeMem(dataOwner);
hFreeConn(&conn);
}

