/* hgdpClick -- handlers for Human Genome Variation Project tracks */

#include "common.h"
#include "hgc.h"
#include "hgdpGeo.h"
#include "hgConfig.h"
#include "trashDir.h"
#include "pipeline.h"
#include "obscure.h"
#include "htmshell.h"

static char const rcsid[] = "$Id: hgdpClick.c,v 1.9 2010/05/11 01:43:28 kent Exp $";

struct hgdpPopInfo
    {
    char *name;
    float longitude;
    float latitude;
    };

// auto-generated -- see makeDb/doc/hgdpGeo.txt
struct hgdpPopInfo pops[] =
    {
    { "Adygei", 44, 39 },
    { "Balochi", 30, 73 },
    { "BantuKenya", -3, 37 },
    { "BantuSouthAfrica", -25.56926433, 24.25 },
    { "Basque", 43, 0 },
    { "Bedouin", 29, 40 },
    { "BiakaPygmy", 4, 17 },
    { "Brahui", 32, 63 },
    { "Burusho", 39, 78 },
    { "Cambodian", 12, 105 },
    { "Colombian", 3, -68 },
    { "Dai", 20, 97 },
    { "Daur", 46.5, 122 },
    { "Druze", 30, 30 },
    { "French", 47, 1 },
    { "Han", 31, 116 },
    { "Han-NChina", 35, 114 },
    { "Hazara", 37, 65 },
    { "Hezhen", 47.4976192, 133.5 },
    { "Italian", 46, 10 },
    { "Japanese", 38, 138 },
    { "Kalash", 42, 68 },
    { "Karitiana", -7, -66 },
    { "Lahu", 22, 103 },
    { "Makrani", 26, 62 },
    { "Mandenka", 12, -12 },
    { "Maya", 19, -91 },
    { "MbutiPygmy", 1, 29 },
    { "Melanesian", -6, 155 },
    { "Miao", 27, 110 },
    { "Mongola", 45, 111 },
    { "Mozabite", 32, 3 },
    { "Naxi", 26, 97 },
    { "Orcadian", 59, -3 },
    { "Oroqen", 52, 128 },
    { "Palestinian", 35, 38 },
    { "Papuan", -4, 143 },
    { "Pathan", 34, 77 },
    { "Pima", 29, -108 },
    { "Russian", 61, 40 },
    { "San", -21, 20 },
    { "Sardinian", 39, 7 },
    { "She", 26, 120 },
    { "Sindhi", 24.49, 69 },
    { "Surui", -14, -59 },
    { "Tu", 37, 100 },
    { "Tujia", 31.4, 108.5 },
    { "Tuscan", 40.5, 14 },
    { "Uygur", 44, 76.5 },
    { "Xibo", 43.497, 84 },
    { "Yakut", 62.98287845, 129.5 },
    { "Yi", 28, 103 },
    { "Yoruba", 7.995094727, 5 }
    };

// PostScript printf format for adding labels to map image.
// auto-generated -- see makeDb/doc/hgdpGeo.txt
static const char *hgdpGeoLabelFormat =
"PSL_font_encode 0 get 0 eq {ISOLatin1+_Encoding /Helvetica /Helvetica PSL_reencode PSL_font_encode 0 1 put} if\n"
"0 setlinecap\n"
"0 setlinejoin\n"
"10 setmiterlimit\n"
"-2175 -1425 T\n"
"\n"
"%%%% PostScript produced by:\n"
"%%%%GMT:  pstext -Jx1i -R0/9/0/6.5 -x-7.25i -y-4.75i -O -K\n"
"S 0 W\n"
"S 0 A\n"
"S V\n"
"0 0 M\n"
"2700 0 D\n"
"0 1950 D\n"
"-2700 0 D\n"
"P\n"
"eoclip N\n"
"PSL_font_encode 1 get 0 eq {ISOLatin1+_Encoding /Helvetica-Bold /Helvetica-Bold PSL_reencode PSL_font_encode 1 1 put} if\n"
"0 0 M 58 F1 (SNP: %s) E /PSL_dimx exch def YP /PSL_dimy exch def\n"
"75 1875 M 0 PSL_dimy neg G 58 F1 (SNP: %s) Z\n"
"S U\n"
"S [] 0 B\n"
"S 0 A\n"
"PSL_font_encode 0 get 0 eq {ISOLatin1+_Encoding /Helvetica /Helvetica PSL_reencode PSL_font_encode 0 1 put} if\n"
"0 setlinecap\n"
"0 setlinejoin\n"
"10 setmiterlimit\n"
"\n"
"%%%% PostScript produced by:\n"
"%%%%GMT:  pstext -Jx1i -R0/9/0/6.5 -O -K -Gblue\n"
"S 0 W\n"
"S 0 A\n"
"S V\n"
"0 0 M\n"
"2700 0 D\n"
"0 1950 D\n"
"-2700 0 D\n"
"P\n"
"eoclip N\n"
"PSL_font_encode 1 get 0 eq {ISOLatin1+_Encoding /Helvetica-Bold /Helvetica-Bold PSL_reencode PSL_font_encode 1 1 put} if\n"
"S 0 0 1 C\n"
"0 0 M 58 F1 (Ancestral Allele: %c) E /PSL_dimx exch def YP /PSL_dimy exch def\n"
"75 1800 M 0 PSL_dimy neg G 58 F1 (Ancestral Allele: %c) Z\n"
"S U\n"
"S [] 0 B\n"
"S 0 A\n"
"PSL_font_encode 0 get 0 eq {ISOLatin1+_Encoding /Helvetica /Helvetica PSL_reencode PSL_font_encode 0 1 put} if\n"
"0 setlinecap\n"
"0 setlinejoin\n"
"10 setmiterlimit\n"
"\n"
"%%%% PostScript produced by:\n"
"%%%%GMT:  pstext -Jx1i -R0/9/0/6.5 -O -K -Gorange\n"
"S 0 W\n"
"S 0 A\n"
"S V\n"
"0 0 M\n"
"2700 0 D\n"
"0 1950 D\n"
"-2700 0 D\n"
"P\n"
"eoclip N\n"
"PSL_font_encode 1 get 0 eq {ISOLatin1+_Encoding /Helvetica-Bold /Helvetica-Bold PSL_reencode PSL_font_encode 1 1 put} if\n"
"S 1 0.647 0 C\n"
"0 0 M 58 F1 (Derived Allele: %c) E /PSL_dimx exch def YP /PSL_dimy exch def\n"
"75 1725 M 0 PSL_dimy neg G 58 F1 (Derived Allele: %c) Z\n"
"S U\n"
"S [] 0 B\n"
"S 0 A\n";

// Parameters for running the Generic Mapping Tools psxy command to plot pie charts
// on two maps in one image: Africa+EurAsia (AEA) and Central/South America (AM).
// These are from scripts written by John Novembre, jnovembre at ucla.
#define PSXY_PIX "-W0.5p"
#define PSXY_CIRCLE "-Sc"
#define PSXY_WEDGE "-Sw"
#define PSXY_CONT_PLOT "-O"
#define PSXY_CONT_PS "-K"
#define AEA_REGION "-R-20/160/-35/72"
#define AEA_PROJ "-JKs70/7.5i"
#define AEA_X "-X0.4i"
#define AEA_Y "-Y0.5i"
#define AM_REGION "-R245/310/-20/35"
#define AM_PROJ "-JKs275.5/1.5i"
#define AM_X "-X6.85i"
#define AM_Y "-Y4.25i"
static char *psxyOrangeAeaCmd[] = 
    {"", "", AEA_REGION, AEA_PROJ, PSXY_PIX, PSXY_CONT_PLOT, PSXY_CONT_PS,
     PSXY_CIRCLE, "-Gorange", AEA_X, AEA_Y, NULL};
static char *psxyPieAeaCmd[] =
    {"", "", AEA_REGION, AEA_PROJ, PSXY_PIX, PSXY_CONT_PLOT, PSXY_CONT_PS,
     PSXY_WEDGE, "-Gblue", NULL};
static char *psxyBlueAeaCmd[] =
    {"", "", AEA_REGION, AEA_PROJ, PSXY_PIX, PSXY_CONT_PLOT, PSXY_CONT_PS,
     PSXY_CIRCLE, "-Gblue", NULL};
static char *psxyOrangeAmCmd[] = 
    {"", "", AM_REGION, AM_PROJ, PSXY_PIX, PSXY_CONT_PLOT, PSXY_CONT_PS,
     PSXY_CIRCLE, "-Gorange", AM_X, AM_Y, NULL};
static char *psxyPieAmCmd[] =
    {"", "", AM_REGION, AM_PROJ, PSXY_PIX, PSXY_CONT_PLOT, PSXY_CONT_PS,
     PSXY_WEDGE, "-Gblue", NULL};
static char *psxyBlueAmCmd[] =
    {"", "", AM_REGION, AM_PROJ, PSXY_PIX, PSXY_CONT_PLOT,
     PSXY_CIRCLE, "-Gblue", NULL};

static void runCommandAppend(char *cmd[], char *program, char *inFile, char *outFile)
/* Use the pipeline module to run a single command pipelineWrite|pipelineAppend. */
{
char **cmds[2];
cmds[0] = cmd;
cmds[1] = NULL;
cmd[0] = program;
cmd[1] = inFile;
struct pipeline *pl = pipelineOpen(cmds, pipelineWrite|pipelineAppend, outFile, NULL);
pipelineWait(pl);
pipelineFree(&pl);
}

static void mustRename(char *oldName, char *newName)
/* Rename a file or errAbort if there's a problem. */
{
if (rename(oldName, newName))
    errAbort("Cannot rename %s to %s", oldName, newName);
}

static void removeDir(char *dirName)
/* Remove all files in a directory and the directory itself. */
// NOTE: If this is ever to be libified, beef it up:
// 1. Add recursion to subdirs (maybe enabled by a new boolean arg)
// 2. Make sure dirName exists
// 3. Make sure current dir is not dirName or a subdir of dirName
{
struct slName *file;
struct dyString *dy = dyStringNew(0);
for (file = listDir(dirName, "*");  file != NULL;  file = file->next)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "%s/%s", dirName, file->name);
    if (unlink(dy->string) != 0)
	errAbort("unlink failed for file %s: %d", dy->string, errno);
    }
if (rmdir(dirName) != 0)
    errAbort("rmdir failed for dir %s: %d", dirName, errno);
}

static void mustChdir(char *dir)
/* Change directory to dir or die trying. */
{
if (chdir(dir) != 0)
    errAbort("chdir(%s) failed: %s", dir, strerror(errno));
}

static void generateImgFiles(struct hgdpGeo *geo, char finalEpsFile[PATH_LEN],
			     char finalPdfFile[PATH_LEN], char finalPngFile[PATH_LEN])
/* Using the frequencies given in geo and the population latitude and longitude
 * given above, plot allele frequency pie charts for each population on a world map.
 * Work in a temporary trash dir and then move result files to the given paths. */
{
// The Generic Mapping Tools commands must have a writeable ./ and $HOME.
// Use trashDirFile as a directory name, cd to that and work there.
char cwd[PATH_LEN];
if (getcwd(cwd, sizeof(cwd)) == NULL)
    errAbort("PATH_LEN (%d) is too short.", PATH_LEN);
struct tempName dirTn;
trashDirFile(&dirTn, "hgc", "hgdpGeo", "");
makeDirs(dirTn.forCgi);
mustChdir(dirTn.forCgi);
char *realHome = getenv("HOME");
setenv("HOME", ".", TRUE);

// Make trash files with coordinate specs for pie charts and full circles:
char rootName[FILENAME_LEN];
splitPath(dirTn.forCgi, NULL, rootName, NULL);
char pieFile[FILENAME_LEN], circleFile[FILENAME_LEN];
safef(pieFile, sizeof(pieFile), "%s_pie.txt", rootName);
safef(circleFile, sizeof(circleFile), "%s_circle.txt", rootName);
FILE *fPie = mustOpen(pieFile, "w");
FILE *fCir = mustOpen(circleFile, "w");
int i;
for (i = 0;  i < HGDPGEO_POP_COUNT;  i++)
    {
    FILE *f = (geo->popFreqs[i] == 1.0) ? fCir : fPie;
    fprintf(f, "%.4f %.4f 0.65 0 %.4f\n",
	    pops[i].latitude, pops[i].longitude, geo->popFreqs[i]*360);
    }
fclose(fPie);
fclose(fCir);

// Build up an EPS image: copy a baseline EPS (map background), add SNP and allele labels,
// and use ps2xy commands to allele frequency pie charts.
char epsFile[FILENAME_LEN];
safef(epsFile, sizeof(epsFile), "%s.eps", rootName);
char mapBgEps[PATH_LEN];
safef(mapBgEps, sizeof(mapBgEps), "%s/hgcData/hgdpGeoMap.eps", cwd);
copyFile(mapBgEps, epsFile);
FILE *fEps = mustOpen(epsFile, "a");
fprintf(fEps, hgdpGeoLabelFormat, geo->name, geo->name,
	geo->ancestralAllele, geo->ancestralAllele, geo->derivedAllele, geo->derivedAllele);
fclose(fEps);
//- run psxy on circle/pie spec trash files for AfrEurAsia and America
char *psxy = cfgOption("hgc.psxyPath");
if (isEmpty(psxy))
    errAbort("How did this get called?  hg.conf doesn't have hgc.psxyPath.");
runCommandAppend(psxyOrangeAeaCmd, psxy, pieFile, epsFile);
runCommandAppend(psxyPieAeaCmd, psxy, pieFile, epsFile);
runCommandAppend(psxyBlueAeaCmd, psxy, circleFile, epsFile);
runCommandAppend(psxyOrangeAmCmd, psxy, pieFile, epsFile);
runCommandAppend(psxyPieAmCmd, psxy, pieFile, epsFile);
runCommandAppend(psxyBlueAmCmd, psxy, circleFile, epsFile);

// Make PDF and PNG:
struct pipeline *pl;
char pdfFile[FILENAME_LEN], pngFile[FILENAME_LEN];
safef(pdfFile, sizeof(pdfFile), "%s.pdf", rootName);
safef(pngFile, sizeof(pngFile), "%s.png", rootName);
char *ps2pdfCmd[] = {"ps2pdf", epsFile, pdfFile, NULL};
char **cmdsPdf[] = {ps2pdfCmd, NULL};
pl = pipelineOpen(cmdsPdf, pipelineWrite, "/dev/null", NULL);
pipelineWait(pl);
pipelineFree(&pl);

char *ps2raster = cfgOption("hgc.ps2rasterPath");
char *ghostscript = cfgOption("hgc.ghostscriptPath");
char gsOpt[PATH_LEN];
safef(gsOpt, sizeof(gsOpt), "-G%s", ghostscript);
char *ps2RasterPngCmd[] = {ps2raster, gsOpt, "-P", "-A", "-Tg", "-E150", epsFile, NULL};
char **cmdsPng[] = {ps2RasterPngCmd, NULL};
pl = pipelineOpen(cmdsPng, pipelineRead, "/dev/null", NULL);
pipelineWait(pl);
pipelineFree(&pl);

// Back to our usual working directory and $HOME:
if (realHome == NULL)
    unsetenv("HOME");
else
    setenv("HOME", realHome, TRUE);
mustChdir(cwd);

// Move the result files into place:
char tmpPath[PATH_LEN];
safef(tmpPath, sizeof(tmpPath), "%s/%s", dirTn.forCgi, epsFile);
mustRename(tmpPath, finalEpsFile);
safef(tmpPath, sizeof(tmpPath), "%s/%s", dirTn.forCgi, pdfFile);
mustRename(tmpPath, finalPdfFile);
safef(tmpPath, sizeof(tmpPath), "%s/%s", dirTn.forCgi, pngFile);
mustRename(tmpPath, finalPngFile);

// Clean up the temporary working directory (trash cleaner script doesn't
// remove empty dirs:
removeDir(dirTn.forCgi);
}


static boolean canMakeImages()
/* Determine whether we have the necessary command line programs for
 * creating images above. */
{
char *psxy = cfgOption("hgc.psxyPath");
char *ps2raster = cfgOption("hgc.ps2rasterPath");
char *ghostscript = cfgOption("hgc.ghostscriptPath");
if (isEmpty(psxy) || isEmpty(ps2raster) || isEmpty(ghostscript))
    return FALSE;
if (!fileExists(psxy) || !fileExists(ps2raster) || !fileExists(ghostscript))
    return FALSE;
return TRUE;
}

static void getTrashFileNames(char *rsId, char epsFile[PATH_LEN], char pdfFile[PATH_LEN],
			      char pngFile[PATH_LEN])
/* Get stable trash file names (always in trash/hgc, based on geo->name so we can reuse). */
{
struct tempName tn;
trashDirFile(&tn, "hgc", "", "");
char trashDir[FILENAME_LEN];
splitPath(tn.forCgi, trashDir, NULL, NULL);
safef(epsFile, PATH_LEN, "%shgdpGeo_%s.eps", trashDir, rsId);
safef(pdfFile, PATH_LEN, "%shgdpGeo_%s.pdf", trashDir, rsId);
safef(pngFile, PATH_LEN, "%shgdpGeo_%s.png", trashDir, rsId);
}

char *hgdpPngFilePath(char *rsId)
/* Return the stable PNG trash-cached image path for rsId. */
{
char epsFile[PATH_LEN], pdfFile[PATH_LEN], pngFile[PATH_LEN];
getTrashFileNames(rsId, epsFile, pdfFile, pngFile);
return cloneString(pngFile);
}

void hgdpGeoImg(struct hgdpGeo *geo)
/* Generate image as PNG, PDF, EPS: world map with pie charts for population allele frequencies. */
{
if (! canMakeImages())
    return;
char geoSnpEpsFile[PATH_LEN], geoSnpPdfFile[PATH_LEN], geoSnpPngFile[PATH_LEN];
getTrashFileNames(geo->name, geoSnpEpsFile, geoSnpPdfFile, geoSnpPngFile);
if (! (fileExists(geoSnpEpsFile) && fileExists(geoSnpPdfFile) && fileExists(geoSnpPngFile)))
    generateImgFiles(geo, geoSnpEpsFile, geoSnpPdfFile, geoSnpPngFile);

printf("<A HREF=\"%s\" TARGET=_BLANK><IMG SRC=\"%s\" WIDTH=677 HEIGHT=490></A><BR>\n",
       geoSnpPngFile, geoSnpPngFile);
printf("<A HREF=\"%s\" TARGET=_BLANK>PDF</A>&nbsp;&nbsp;", geoSnpPdfFile);
printf("<A HREF=\"%s\" TARGET=_BLANK>EPS</A>&nbsp;&nbsp;", geoSnpEpsFile);
printf("<A HREF=\"%s\" TARGET=_BLANK>PNG</A><BR>\n", geoSnpPngFile);
printf("Population key:\n");
printf("<A HREF=\"../images/hgdpGeoPopKey.pdf\" TARGET=_BLANK>PDF</A>&nbsp;&nbsp;");
printf("<A HREF=\"../images/hgdpGeoPopKey.eps\" TARGET=_BLANK>EPS</A>&nbsp;&nbsp;");
printf("<A HREF=\"../images/hgdpGeoPopKey.png\" TARGET=_BLANK>PNG</A><BR>\n");
}

void hgdpGeoFreqTable(struct hgdpGeo *geo)
/* Print an HTML table of populations and allele frequencies. */
{
int i;
printf("<B>Ancestral Allele Frequencies for %s:</B>\n", geo->name);
printf("<TABLE CELLPADDING=1 CELLSPACING=0>\n");
for (i = 0;  i < HGDPGEO_POP_COUNT;  i++)
    {
    printf("<TR><TD>%s</TD><TD>%.4f</TD></TR>\n",
	   pops[i].name, geo->popFreqs[i]);
    }
printf("</TABLE>\n");
}

void doHgdpGeo(struct trackDb *tdb, char *item)
/* Show details page for HGDP SNP with population allele frequencies
 * plotted on a world map. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[512];
struct sqlResult *sr;
char **row;
int start = cartInt(cart, "o");
genericHeader(tdb, item);
int hasBin=1;

safef(query, sizeof(query),
      "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
      tdb->table, item, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("doHgdpGeo: no match in %s for %s at %s:%d", tdb->table, item, seqName, start);
struct hgdpGeo *geo = hgdpGeoLoad(row+hasBin);
sqlFreeResult(&sr);
printCustomUrl(tdb, item, TRUE);
bedPrintPos((struct bed *)geo, 4, tdb);
printf("<B>Ancestral Allele:</B> %c<BR>\n", geo->ancestralAllele);
printf("<B>Derived Allele:</B> %c<BR>\n", geo->derivedAllele);
printOtherSnpMappings(tdb->table, item, start, conn, hasBin);
printf("<BR>\n");
printf("<TABLE><TR><TD>\n");
hgdpGeoFreqTable(geo);
printf("</TD><TD valign=top>\n");
hgdpGeoImg(geo);
printf("</TD></TR></TABLE>\n");
printTrackHtml(tdb);
hFreeConn(&conn);
}

