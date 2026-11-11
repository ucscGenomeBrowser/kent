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
#include "hgHgvs.h"
#include "variantProjector.h"
#include "genePred.h"
#include "genbank.h"
#include "bigBed.h"
#include "chromAlias.h"

static char *positionText(char *chrom, int chromStart, int chromEnd)
/* Browser-style position string for a BED range, so the start reads one higher
 * than the stored coordinate.  Plain digits, no group separators: this string
 * is what the edit box hands back, and the position box in the hgTracks dialog
 * writes the same shape.  Caller frees. */
{
char spec[256];
safef(spec, sizeof(spec), "%s:%d-%d", chrom, chromStart + 1, chromEnd);
return cloneString(spec);
}

static void makeVarcharTextVar(char *varName, char *initialVal, int charSize)
/* Make a text control for a varchar(255) column, which takes at most 255
 * characters. */
{
cgiMakeTextVar(varName, initialVal, charSize);
jsInlineF("document.getElementById('%s').maxLength = 255;\n", varName);
}

static struct genePred *getGeneTrackOverlaps(struct trackDb *geneTdb, char *db, char *chrom,
                                             int start, int end)
/* Return transcripts in geneTdb overlapping chrom:start-end, from a bigGenePred file
 * or a genePred SQL table. */
{
struct genePred *gpList = NULL;
if (sameString(geneTdb->type, "bigGenePred"))
    {
    char *fileName = hReplaceGbdb(trackDbSetting(geneTdb, "bigDataUrl"));
    if (isEmpty(fileName))
        return NULL;
    struct bbiFile *bbi = bigBedFileOpenAlias(fileName, chromAliasFindAliases);
    struct lm *lm = lmInit(0);
    struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
    for (bb = bbList;  bb != NULL;  bb = bb->next)
        {
        struct genePredExt *gpx = genePredFromBigGenePred(chrom, bb);
        if (gpx != NULL)
            {
            /* name2 is not the gene symbol in every bigGenePred (in MANE it is
             * not); geneName2 is the human-readable one when the file has it. */
            if (isNotEmpty(gpx->geneName2))
                gpx->name2 = gpx->geneName2;
            slAddHead(&gpList, (struct genePred *)gpx);
            }
        }
    lmCleanup(&lm);
    bigBedFileClose(&bbi);
    freeMem(fileName);
    }
else
    {
    struct sqlConnection *conn = hAllocConn(db);
    int rowOffset = 0;
    struct sqlResult *sr = hRangeQuery(conn, geneTdb->table, chrom, start, end, NULL, &rowOffset);
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
        slAddHead(&gpList, genePredLoad(row + rowOffset));
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
slReverse(&gpList);
return gpList;
}

static void printMyVariantsHgvs(struct myVariants *item)
/* For SNV items, show the genomic HGVS g. term and a c./n. term for each overlapping
 * transcript in the assembly's default (MANE-first) gene track. */
{
if (!sameOk(item->itemType, "snv"))
    return;
/* With no alleles there is no change to describe, and an empty alt would read
 * as a deletion. */
if (isEmpty(item->ref) && isEmpty(item->alt))
    return;
char *db = item->db;
struct seqWindow *gSeqWin = chromSeqWindowNew(db, NULL, 0, 0);
struct bed3 variantBed;
ZeroVar(&variantBed);
variantBed.chrom = item->chrom;
variantBed.chromStart = item->chromStart;
variantBed.chromEnd = item->chromEnd;
char *chromAcc = hRefSeqAccForChrom(db, item->chrom);
/* vpGenomicToTranscript requires an IUPAC alt (or "*"/"<DEL>") and aborts otherwise, and
 * rewrites "<DEL>" in place; work on a copy, normalize "<DEL>", and skip transcript terms
 * for any alt it can't project. */
char *alt = cloneString(item->alt);
boolean altProjectable = (isAllNt(alt, strlen(alt)) ||
                          sameString(alt, "*") || sameString(alt, "<DEL>"));
if (sameString(alt, "<DEL>"))
    alt[0] = '\0';
char *hgvsG = hgvsGFromVariant(gSeqWin, &variantBed, alt, chromAcc, FALSE);

struct trackDb *geneTdb = altProjectable ? hgvsDefaultGeneTrack(db) : NULL;
if (geneTdb != NULL)
    htmlPrintf("<B>HGVS</B> (relative to %s):<BR>\n", geneTdb->shortLabel);
else
    printf("<B>HGVS:</B><BR>\n");
if (isNotEmpty(hgvsG))
    htmlPrintf("&nbsp;&nbsp;%s<BR>\n", hgvsG);

if (geneTdb == NULL)
    return;

struct genePred *gp, *gpList = getGeneTrackOverlaps(geneTdb, db, item->chrom,
                                                    item->chromStart, item->chromEnd);
for (gp = gpList;  gp != NULL;  gp = gp->next)
    {
    char *seq = txSeqFromGp(db, gp);
    struct dnaSeq *txSeq = newDnaSeq(seq, strlen(seq), gp->name);
    struct genbankCds cds;
    genePredToCds(gp, &cds);
    struct psl *psl = genePredToPsl(gp, hChromSize(db, gp->chrom), txSeq->size);
    vpExpandIndelGaps(psl, gSeqWin, txSeq);
    struct vpTx *vpTx = vpGenomicToTranscript(gSeqWin, &variantBed, alt, psl, txSeq);
    char *hgvsTx = NULL;
    if (cds.end > cds.start)
        hgvsTx = hgvsCFromVpTx(vpTx, gSeqWin, psl, &cds, txSeq, FALSE);
    else
        hgvsTx = hgvsNFromVpTx(vpTx, gSeqWin, psl, txSeq, FALSE);
    /* hgvsTx already starts with the transcript accession, so only the gene
     * symbol is added, and only when the track carries one distinct from it. */
    if (isNotEmpty(hgvsTx))
        {
        if (isNotEmpty(gp->name2) && !sameString(gp->name2, gp->name))
            htmlPrintf("&nbsp;&nbsp;%s (%s)<BR>\n", hgvsTx, gp->name2);
        else
            htmlPrintf("&nbsp;&nbsp;%s<BR>\n", hgvsTx);
        }
    }
}

static struct slName *allCustomFields(char *table)
/* Return the names of table's custom fields, shown and hidden, the hidden ones
 * without their _hidden_ prefix. */
{
struct slName *shown = NULL, *hidden = NULL;
myVariantsGetFieldLists(table, &shown, &hidden);
return slCat(shown, hidden);
}

static struct slName *customFieldsWithValues(struct sqlConnection *conn, char *table,
    unsigned itemId)
/* Return the custom fields of table, shown or hidden, that hold a value for the
 * item with itemId.  A hidden field is named without its _hidden_ prefix. */
{
struct slName *shown = NULL, *hidden = NULL, *fld;
myVariantsGetFieldLists(table, &shown, &hidden);
struct slName *withValues = NULL;
struct slName *fields = slCat(shown, hidden);
if (fields == NULL)
    return NULL;
struct dyString *query = sqlDyStringCreate("select ");
boolean inHidden = FALSE;   /* The hidden fields follow the shown ones. */
for (fld = fields; fld != NULL; fld = fld->next)
    {
    if (fld == hidden)
        inHidden = TRUE;
    if (fld != fields)
        sqlDyStringPrintf(query, ", ");
    /* Quoted, because a custom field can be named order. */
    if (inHidden)
        sqlDyStringPrintf(query, "`_hidden_%s`", fld->name);
    else
        sqlDyStringPrintf(query, "`%s`", fld->name);
    }
sqlDyStringPrintf(query, " from %s where id=%u", table, itemId);
struct sqlResult *sr = sqlGetResult(conn, dyStringContents(query));
char **row = sqlNextRow(sr);
if (row != NULL)
    {
    int i = 0;
    for (fld = fields; fld != NULL; fld = fld->next, i++)
        if (isNotEmpty(row[i]))
            slNameAddHead(&withValues, fld->name);
    }
sqlFreeResult(&sr);
dyStringFree(&query);
slFreeList(&fields);
slReverse(&withValues);
return withValues;
}

static void myVariantsPrintMoveToProject(char *trackName,
    struct myVariantsProject *thisProject, char *thisTable, unsigned itemId,
    struct sqlConnection *conn)
/* Print the control that moves this annotation to another of the owner's
 * projects.  Each option carries the custom fields this annotation has a value
 * in that the project does not have, so hgc.js can say what a move would drop
 * before the user submits. */
{
char *userName = getUserName();
struct myVariantsProject *projects = myVariantsListProjects(userName), *proj;
if (projects == NULL)
    {
    htmlPrintf("%s\n", emptyForNull(thisProject ? thisProject->label : NULL));
    myVariantsProjectFreeList(&projects);
    return;
    }
struct slName *ourFields = customFieldsWithValues(conn, thisTable, itemId);

char selectName[128], varName[128];
safef(selectName, sizeof(selectName), "%s_projectSelect", trackName);
safef(varName, sizeof(varName), "%s_%s", trackName, "moveTo");
htmlPrintf("<select name='%s|attr|' id='%s|attr|'>", varName, selectName);
for (proj = projects; proj != NULL; proj = proj->next)
    {
    /* Which of this annotation's custom fields the destination has no column
     * for.  Empty for the project it is already in. */
    struct dyString *missing = dyStringNew(0);
    if (!sameString(proj->dbTable, thisTable))
        {
        struct slName *theirFields = allCustomFields(proj->dbTable), *fld;
        for (fld = ourFields; fld != NULL; fld = fld->next)
            {
            if (slNameFind(theirFields, fld->name) == NULL)
                {
                if (dyStringLen(missing) > 0)
                    dyStringAppend(missing, ", ");
                dyStringAppend(missing, fld->name);
                }
            }
        slFreeList(&theirFields);
        }
    boolean isCurrent = sameString(proj->dbTable, thisTable);
    htmlPrintf("<option value='%u' data-missing='%s|attr|'%s|none|>%s</option>",
        proj->id, dyStringContents(missing), isCurrent ? " selected" : "", proj->label);
    dyStringFree(&missing);
    }
/* A brand new project is given this annotation's custom fields as it is created,
 * so moving into one drops nothing. */
htmlPrintf("<option value='" MYVARIANTS_NEW_PROJECT "' data-missing=''>"
           "New track...</option>");
printf("</select> ");
char newName[128];
safef(newName, sizeof(newName), "%s_%s", trackName, "moveToNew");
htmlPrintf("<input type='text' name='%s|attr|' id='%s|attr|'"
    " style='display:none' placeholder='New track name'>", newName, newName);
/* The |attr| specifier has to sit right after its %s, so each id is built
 * first rather than spelled out in the format string. */
char idName[128];
/* Confirms the typed name by putting it in the dropdown, so the choice reads
 * the same way picking an existing project does. */
safef(idName, sizeof(idName), "%s_moveSave", trackName);
htmlPrintf(" <button type='button' id='%s|attr|' style='display:none'>Save</button>", idName);
safef(idName, sizeof(idName), "%s_moveSaveMsg", trackName);
htmlPrintf("<span id='%s|attr|' style='color:#666; font-size:12px'></span>", idName);
safef(idName, sizeof(idName), "%s_moveWarn", trackName);
htmlPrintf("<span id='%s|attr|' style='color:#a00'></span>", idName);
slFreeList(&ourFields);
myVariantsProjectFreeList(&projects);
}

void doMyVariantsDetails(struct customTrack *ct, char *itemIdString)
/* Show details of a myVariants item. */
{
jsIncludeFile("hgc.js",NULL);
char *idString = cloneString(itemIdString);
char *trackName = ct->tdb->track;

/* The track carries the table it reads, and it only exists when the viewer is
 * allowed to read it: an own track when the project is theirs, a shared track
 * when its share still resolves.  So the table needs no further checking; only
 * the write permission does. */
boolean isShared = isMyVariantsSharedTrack(trackName);
int permission = MYVAR_PERM_READONLY;
char *dataOwner = NULL;     /* who owns the annotation, for the shared banner */
if (isShared)
    {
    struct myVariantsShare *share = myVariantsResolveSharedTrack(trackName, cart);
    if (share == NULL)
        {
        printf("Share is no longer available.\n");
        return;
        }
    permission = share->permission;
    dataOwner = cloneString(share->ownerUser);
    myVariantsShareFree(&share);
    }
else
    dataOwner = cloneString(getUserName());

/* Anon users never edit shared items, even when the share is read-write. */
boolean canEdit = !isShared || (permission == MYVAR_PERM_READWRITE && getUserName() != NULL);

char *tableName = ct->dbTableName;
if (isEmpty(tableName))
    {
    printf("This annotation track is no longer available.\n");
    return;
    }

/* The project this annotation is in.  Its label is what the page shows, and
 * for an own track it is the entry preselected in the move control. */
struct sqlConnection *central = hConnectCentral();
struct myVariantsProject *thisProject = NULL;
if (sqlTableExists(central, MYVARIANTS_PROJECT_TABLE))
    thisProject = myVariantsProjectByDbTable(central, tableName);
hDisconnectCentral(&central);
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
        else if (permission == MYVAR_PERM_READWRITE
                 && (loginSystemEnabled() || wikiLinkEnabled()))
            /* A logged-out recipient of an edit link. */
            printf("<div style='padding:6px; margin-bottom:8px; background:#e3f2fd; "
                "border:1px solid #90caf9; border-radius:4px'>"
                "<B>Shared from %s (read-only)</B> This link lets you edit. "
                "<A HREF='%s'>Log in</A> to make changes.</div>\n",
                htmlEncode(dataOwner), wikiLinkUserLoginUrl(cartSessionId(cart)));
        else
            printf("<div style='padding:6px; margin-bottom:8px; background:#e3f2fd; "
                "border:1px solid #90caf9; border-radius:4px'>"
                "<B>Shared from %s (read-only)</B></div>\n", htmlEncode(dataOwner));
        }

    if (canEdit)
        {
        boolean isTranscript = sameOk(item->itemType, "transcript");
        boolean isCnv = sameOk(item->itemType, "cnv");
        boolean isOther = sameOk(item->itemType, "other");
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

        /* Annotation type.  Every type gets the same control and the same set of
         * fields, shown and hidden by type the way the create dialog does it, so
         * the type is both visible and changeable after creation. */
        safef(varName, sizeof(varName), "%s_%s", trackName, "itemType");
        printf("<div id=\"mvTypeBar\" style=\"margin-bottom:12px; padding:8px 10px; "
               "background:#f7f7f7; border:1px solid #ddd;\">\n");
        printf("<div style=\"font-weight:bold; font-size:12px; color:#555; "
               "margin-bottom:4px;\">Annotation type</div>\n<div>\n");
            {
            /* Same order and labels as the create dialog. */
            char *typeValues[] = { "snv", "cnv", "transcript", "other" };
            char *typeLabels[] = { "Short variant (SNV)", "Copy number variant (CNV)",
                                   "Transcript", "Other" };
            char *current = myVariantsCanonicalItemType(item->itemType);
            if (current == NULL)
                current = "snv";
            int ti;
            for (ti = 0; ti < ArraySize(typeValues); ti++)
                htmlPrintf("<label style='margin-right:20px;'>"
                    "<input type='radio' name='%s|attr|' class='mvTypeRadio' "
                    "value='%s|attr|'%s|none|> %s</label>\n",
                    varName, typeValues[ti],
                    sameString(current, typeValues[ti]) ? " checked" : "",
                    typeLabels[ti]);
            }
        printf("</div>\n</div>\n");

        /* Put up editable label. */
        safef(varName, sizeof(varName), "%s_%s", trackName, "name");
        printf("<B>Label:</B> ");
        makeVarcharTextVar(varName, item->name, 17);
        printInfoIcon("A short label for this annotation, displayed in the browser.");
        printf("<BR>\n");

        /* Put up editable description. */
        safef(varName, sizeof(varName), "%s_%s", trackName, "description");
        printf("<B>Description:</B> ");
        printInfoIcon("Longer notes or comments about this annotation. Displayed on this details page.");
        printf("<BR>\n");
        cgiMakeTextArea(varName, item->description, 8, 80);
        printf("<BR>\n");

        /* Editable position.  The chromosome travels in the string, so there is
         * no separate chromosome line.  applyItemEdits in hgTracks parses it
         * again and refuses a range that is not on the assembly. */
        char posVarName[128];
        safef(posVarName, sizeof(posVarName), "%s_%s", trackName, "position");
        char *posText = positionText(item->chrom, item->chromStart, item->chromEnd);
        printf("<B>Position:</B> ");
        cgiMakeTextVar(posVarName, posText, 30);
        freeMem(posText);
        printInfoIcon("The range this annotation covers, written the way the "
            "browser position box writes it.");
        printf("<BR>\n");
        /* Catch a mistyped position before the form leaves the page, so the
         * reader keeps the rest of what they typed.  Only Update is held up:
         * Cancel and Delete do not care what is in the box, and a browser
         * without event.submitter leaves the check to the server. */
        jsInlineF(
            "$(function(){\n"
            "  var posEl = document.getElementById('%s');\n"
            "  if (!posEl || !posEl.form) { return; }\n"
            "  posEl.form.addEventListener('submit', function(ev){\n"
            "    var by = ev.submitter;\n"
            "    if (!by || by.name !== 'submit') { return; }\n"
            /* A bare sequence name is the whole sequence, and only the server
             * knows how long that is. */
            "    if (/^\\s*[\\w._#-]+\\s*$/.test(posEl.value)) { return; }\n"
            "    var problem = bedRangeProblem(posEl.value);\n"
            "    if (!problem) { return; }\n"
            "    ev.preventDefault();\n"
            "    posEl.style.borderColor = '#a00';\n"
            "    posEl.focus();\n"
            "    alert(problem);\n"
            "  });\n"
            "});\n",
            posVarName);

            {
            /* Transcript-only fields.  Always rendered so the type can be
             * switched here; the browser shows or hides the block. */
            printf("<div id=\"mvTranscriptFields\"%s>\n",
                isTranscript ? "" : " style=\"display:none\"");
            /* Editable CDS Start / CDS End (stored as thickStart/thickEnd). */
            int chromSize = hChromSize(database, item->chrom);
            char chromSizeString[16];
            safef(chromSizeString, sizeof(chromSizeString), "%d", chromSize);
            printf("<B>CDS Start (0-based):</B> ");
            safef(varName, sizeof(varName), "%s_%s", trackName, "thickStart");
            cgiMakeIntVarInRange(varName, item->thickStart, NULL, 80, "0", chromSizeString);
            printInfoIcon("BED thickStart: start of the coding region.");
            printf("<BR>\n");
            printf("<B>CDS End:</B> ");
            safef(varName, sizeof(varName), "%s_%s", trackName, "thickEnd");
            cgiMakeIntVarInRange(varName, item->thickEnd, NULL, 80, "0", chromSizeString);
            printInfoIcon("BED thickEnd: end of the coding region.");
            printf("<BR>\n");

            /* Blocks (BED12). Hidden inputs are kept in sync by the widget;
             * updateBlocksFields in hgTracks reads them on submit. */
            char vBC[128], vBS[128], vBT[128];
            safef(vBC, sizeof(vBC), "%s_blockCount", trackName);
            safef(vBS, sizeof(vBS), "%s_blockSizes", trackName);
            safef(vBT, sizeof(vBT), "%s_chromStarts", trackName);
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
            /* Block offsets are relative to the start of the position box, so the
             * widget reads the range out of that box. */
            jsInlineF(
                "$(function(){\n"
                "  if (typeof myVariantsBlocks === 'undefined') { return; }\n"
                "  var sizesEl = document.getElementById('%s');\n"
                "  var startsEl = document.getElementById('%s');\n"
                "  var posEl = document.getElementById('%s');\n"
                "  var sizes = sizesEl.value ? sizesEl.value.split(',').map(Number) : [];\n"
                "  var starts = startsEl.value ? startsEl.value.split(',').map(Number) : [];\n"
                "  var range = function(){ return parseBedRange(posEl.value); };\n"
                "  myVariantsBlocks.mount('myVariantsBlocksUi', {\n"
                "    initialSizes: sizes,\n"
                "    initialStarts: starts,\n"
                "    getStart: function(){ var r = range(); return r ? r.start : NaN; },\n"
                "    getEnd:   function(){ var r = range(); return r ? r.end : NaN; },\n"
                "    hiddenCountInput:  document.getElementById('%s'),\n"
                "    hiddenSizesInput:  sizesEl,\n"
                "    hiddenStartsInput: startsEl\n"
                "  });\n"
                "});\n",
                vBS, vBT, posVarName, vBC);
            dyStringFree(&sizesCsv);
            dyStringFree(&startsCsv);
            printf("</div>\n");
            }

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

        /* Alleles.  CNV gets a type vocabulary and calls the alt field
         * "Sequence"; SNV gets Ref and Alt; transcript and other get neither.
         * One alt input serves both so the form cannot post two values for it,
         * matching the create dialog. */
        printf("<div id=\"mvCnvFields\"%s>\n", isCnv ? "" : " style=\"display:none\"");
        safef(varName, sizeof(varName), "%s_%s", trackName, "cnvType");
        printf("<B>CNV type:</B> ");
        htmlPrintf("<select name='%s|attr|' id='%s|attr|'>", varName, varName);
            {
            int ci;
            for (ci = 0; ci < myVariantsNumCnvTypes; ci++)
                {
                boolean sel = sameOk(item->cnvType, myVariantsCnvTypes[ci]);
                htmlPrintf("<option value='%s|attr|'%s|none|>%s</option>",
                    myVariantsCnvTypes[ci], sel ? " selected" : "",
                    myVariantsCnvTypes[ci]);
                }
            }
        printf("</select>");
        printInfoIcon("CNV vocabulary follows gnomAD");
        printf("<BR>\n</div>\n");

        printf("<div id=\"mvRefField\"%s>\n", isCnv || isTranscript || isOther ?
               " style=\"display:none\"" : "");
        safef(varName, sizeof(varName), "%s_%s", trackName, "ref");
        printf("<B>Ref:</B> ");
        makeVarcharTextVar(varName, item->ref, 17);
        printInfoIcon("Reference allele sequence at this position.");
        printf("<BR>\n</div>\n");

        printf("<div id=\"mvAltField\"%s>\n", isTranscript || isOther ?
               " style=\"display:none\"" : "");
        printf("<B id=\"mvAltLabel\">%s</B> ", isCnv ? "Sequence:" : "Alt:");
        safef(varName, sizeof(varName), "%s_%s", trackName, "alt");
        makeVarcharTextVar(varName, item->alt, isCnv ? 40 : 17);
        printInfoIcon("Alternate allele, or the inserted or duplicated sequence "
                      "for a copy number variant.");
        printf("<BR>\n</div>\n");

        /* Project: the track this annotation is in.  A recipient sees which
         * project they were given but cannot move rows out of it; the owner can
         * move the annotation to another of their projects, which means another
         * table, so any custom field the destination lacks is dropped. */
        printf("<B>Track:</B> ");
        if (isShared)
            htmlPrintf("%s<BR>\n", emptyForNull(thisProject ? thisProject->label : NULL));
        else
            {
            myVariantsPrintMoveToProject(trackName, thisProject, tableName, item->id, conn);
            printInfoIcon("The track this annotation is in. Pick another track to move it there.");
            printf("<BR>\n");
            }

        /* Mouseover */
        safef(varName, sizeof(varName), "%s_%s", trackName, "mouseover");
        printf("<B>Mouseover:</B> ");
        makeVarcharTextVar(varName, item->mouseover, 60);
        printInfoIcon("Short text shown when hovering over this item.");
        printf("<BR>\n");

        /* Custom fields */
            {
            struct slName *customCols = myVariantsGetCustomFields(tableName);
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
                    /* Quoted, because a custom field can be named order. */
                    sqlDyStringPrintf(cfQuery, "`%s`", col->name);
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
                        makeVarcharTextVar(varName, cfRow[i] ? cfRow[i] : "", 40);
                        printf("<BR>\n");
                        }
                    }
                sqlFreeResult(&cfSr);
                dyStringFree(&cfQuery);
                slFreeList(&customCols);
                }
            }

        /* Show the fields that belong to the type currently picked, so changing
         * the type here behaves the way it does in the create dialog. */
        jsInline(
            "(function() {\n"
            "  var show = function(id, on) {\n"
            "    var el = document.getElementById(id);\n"
            "    if (el) { el.style.display = on ? '' : 'none'; }\n"
            "  };\n"
            "  var applyType = function(type) {\n"
            "    show('mvTranscriptFields', type === 'transcript');\n"
            "    show('mvCnvFields', type === 'cnv');\n"
            "    show('mvRefField', type === 'snv');\n"
            "    show('mvAltField', type === 'snv' || type === 'cnv');\n"
            "    var altLabel = document.getElementById('mvAltLabel');\n"
            "    if (altLabel) { altLabel.textContent = "
                    "type === 'cnv' ? 'Sequence:' : 'Alt:'; }\n"
            "  };\n"
            "  var radios = document.querySelectorAll('.mvTypeRadio');\n"
            "  radios.forEach(function(r) {\n"
            "    r.addEventListener('change', function() { applyType(this.value); });\n"
            "  });\n"
            "})();\n");

        /* Buttons: Update and Cancel always; Delete only for own track.  Delete
         * is last in the row and red, and asks first unless the user has said
         * to stop asking. */
        printf("<div style=\"margin-top:14px\">\n");
        cgiMakeButton("submit", "Update");
        printf(" ");
        safef(varName, sizeof(varName), "%s_%s", trackName, "cancel");
        cgiMakeButton(varName, "Cancel");
        if (!isShared)
            {
            safef(varName, sizeof(varName), "%s_%s", trackName, "delete");
            printf(" ");
            printf("<input type=\"submit\" name=\"%s\" id=\"%s\" value=\"Delete\" "
                   "title=\"Permanently removes this annotation. This cannot be undone.\" "
                   "style=\"color:#a00; border-color:#a00;\">\n", varName, varName);
            printf("</div>\n");
            printf("<div id=\"myVariantsDeleteConfirm\" style=\"display:none; "
                   "margin-top:10px; padding:8px; max-width:420px; "
                   "background:#fff8e1; border:1px solid #ffe082; border-radius:4px\">\n");
            printf("<div>Delete this annotation? This cannot be undone.</div>\n");
            printf("<div style=\"margin-top:8px\">\n");
            printf("<button type=\"button\" id=\"myVariantsDeleteYes\" "
                   "style=\"color:#a00; border-color:#a00\">Delete</button>\n");
            printf("<button type=\"button\" id=\"myVariantsDeleteNo\" "
                   "style=\"margin-left:6px\">Keep it</button>\n");
            printf("<label style=\"margin-left:12px\" "
                   "title=\"Remembered in this browser. Change it on the "
                   "Configure page.\">"
                   "<input type=\"checkbox\" id=\"myVariantsDeleteRemember\"> "
                   "Do not ask again</label>\n");
            printf("</div>\n</div>\n");
            /* The preference is per browser, like the create dialog's navigation
             * choice; the browser is where the click happens.  Both are set
             * from the hgTracks Configure page. */
            jsInlineF(
                "(function() {\n"
                "  var btn = document.getElementById('%s');\n"
                "  var panel = document.getElementById('myVariantsDeleteConfirm');\n"
                "  var confirmed = false;\n"
                "  btn.addEventListener('click', function(ev) {\n"
                "    if (confirmed) { return; }\n"
                "    if (localStorage.getItem('myVariants_deleteNoConfirm') === 'yes') "
                          "{ return; }\n"
                "    ev.preventDefault();\n"
                "    panel.style.display = '';\n"
                "  });\n"
                "  document.getElementById('myVariantsDeleteYes')"
                    ".addEventListener('click', function() {\n"
                "    if (document.getElementById('myVariantsDeleteRemember').checked)\n"
                "      localStorage.setItem('myVariants_deleteNoConfirm', 'yes');\n"
                "    confirmed = true;\n"
                "    btn.click();\n"
                "  });\n"
                "  document.getElementById('myVariantsDeleteNo')"
                    ".addEventListener('click', function() {\n"
                "    panel.style.display = 'none';\n"
                "  });\n"
                "})();\n",
                varName);
            }
        else
            printf("</div>\n");
        printf("</FORM>\n");
        }
    else
        {
        /* Read-only display for shared items without write permission.
         * htmlPrintf escapes %s by default; that prevents stored XSS via
         * owner-controlled fields. */
        htmlPrintf("<B>Label:</B> %s<BR>\n", item->name);
        htmlPrintf("<B>Description:</B><BR>\n%s<BR>\n", item->description);
        char *roPosText = positionText(item->chrom, item->chromStart, item->chromEnd);
        htmlPrintf("<B>Position:</B> %s<BR>\n", roPosText);
        freeMem(roPosText);
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
        boolean roIsCnv = sameOk(item->itemType, "cnv");
        if (roIsCnv && isNotEmpty(item->cnvType))
            htmlPrintf("<B>CNV type:</B> %s<BR>\n", item->cnvType);
        if (isNotEmpty(item->ref))
            htmlPrintf("<B>Ref:</B> %s<BR>\n", item->ref);
        if (isNotEmpty(item->alt))
            htmlPrintf("<B>%s:</B> %s<BR>\n", roIsCnv ? "Sequence" : "Alt", item->alt);
        if (thisProject != NULL)
            htmlPrintf("<B>Track:</B> %s<BR>\n", thisProject->label);
        if (isNotEmpty(item->mouseover))
            htmlPrintf("<B>Mouseover:</B> %s<BR>\n", item->mouseover);

        /* Custom fields read-only */
            {
            struct slName *customCols = myVariantsGetCustomFields(tableName);
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
                    /* Quoted, because a custom field can be named order. */
                    sqlDyStringPrintf(cfQuery, "`%s`", col->name);
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

    printMyVariantsHgvs(item);

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

