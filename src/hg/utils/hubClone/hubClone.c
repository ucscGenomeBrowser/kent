/* hubClone - Clone the hub text files to a local copy, fixing up bigDataUrls
 * to remote location if necessary. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "trackDb.h"
#include "cart.h" // can't include trackHub.h without this?
#include "trackHub.h"
#include "errCatch.h"
#include "ra.h"
#include "hui.h"
#include "pipeline.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubClone - Clone the remote hub text files to a local copy in newDirectoryName, fixing up bigDataUrls to remote location if necessary\n"
  "usage:\n"
  "   hubClone http://url/to/hub.txt\n"
  "options:\n"
  "   -udcDir=/dir/to/udcCache   Path to udc directory\n"
  "   -download                  Download data files in addition to the hub configuration files\n"
  "   -skipMissingAssemblies     Skip assemblies whose trackDb.txt files are missing instead of aborting\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {"download", OPTION_BOOLEAN},
   {"skipMissingAssemblies", OPTION_BOOLEAN},
   {NULL, 0},
};

/* Simple structure to hold genome info when doing manual parsing */
struct simpleGenome
    {
    struct simpleGenome *next;
    char *name;           /* genome name */
    char *trackDbPath;    /* relative path to trackDb file */
    };

void polishHubName(char *name)
/* Helper function for making somewhat safe directory names. Changes non-alpha to '_' */
{
if (name == NULL)
    return;

char *in = name;
char c;

for(; (c = *in) != 0; in++)
    {
    if (!(isalnum(c) || c == '-' || c == '_'))
        *in = '_';
    }
}

void printHubStanza(struct hash *stanza, FILE *out, char *baseUrl)
/* print hub.txt stanza to out */
{
struct hashEl *hel, *helList = hashElListHash(stanza);
fprintf(out, "%s %s\n", "hub", (char *)hashFindVal(stanza, "hub"));
for (hel = helList; hel != NULL; hel = hel->next)
    {
    if (!sameString(hel->name, "hub"))
        {
        if (sameString(hel->name, "descriptionUrl"))
            fprintf(out, "%s %s\n", hel->name, trackHubRelativeUrl(baseUrl, hel->val));
        else
            fprintf(out, "%s %s\n", hel->name, (char *)hel->val);
        }
    }
fprintf(out, "\n");
hashElFreeList(&helList);
}

void printGenomeStanza(struct hash *stanza, FILE *out, char *baseUrl, boolean oneFile)
/* print genomes.txt stanza to out */
{
struct hashEl *hel, *helList = hashElListHash(stanza);
char *genome = (char *)hashFindVal(stanza, "genome");

fprintf(out, "%s %s\n", "genome", genome);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    if (!sameString(hel->name, "genome"))
        {
        if (sameString(hel->name, "groups") ||
            sameString(hel->name, "twoBitPath") ||
            sameString(hel->name, "htmlPath")
            )
            fprintf(out, "%s %s\n", hel->name, trackHubRelativeUrl(baseUrl, hel->val));
        else if (sameString(hel->name, "trackDb"))
            {
            if (oneFile)
                {
                fprintf(out, "%s %s\n", hel->name, (char *)hel->val);
                }
            else
                {
                // some assembly hubs use different directory names than the typical
                // genomeName/trackDb.txt setup, hardcode this so assembly hub will
                // still load locally
                char *tdbFileName = NULL;
                if ((tdbFileName = strrchr((char *)hel->val, '/')) != NULL)
                    tdbFileName += 1;
                else
                    tdbFileName = (char *)hel->val;
                fprintf(out, "%s %s/%s\n", hel->name, genome, tdbFileName);
                }
            }
        else
            fprintf(out, "%s %s\n", hel->name, (char *)hel->val);
        }
    }
fprintf(out, "\n");
hashElFreeList(&helList);
}


void printTrackDbStanza(struct hash *stanza, FILE *out, char *baseUrl, char *downloadDir)
/* print a trackDb stanza but with relative references replaced by remote links */
{
struct hashEl *hel, *helList = hashElListHash(stanza);
struct dyString *fname = dyStringNew(0);
fprintf(out, "%s %s\n", "track", (char *)hashFindVal(stanza, "track"));
for (hel = helList; hel != NULL; hel = hel->next)
    {
    if (!sameString(hel->name, "track"))
        {
        if (sameString(hel->name, "bigDataUrl") ||
            sameString(hel->name, "bigDataIndex") ||
            sameString(hel->name, "barChartMatrixUrl") ||
            sameString(hel->name, "barChartSampleUrl") ||
            sameString(hel->name, "linkDataUrl") ||
            sameString(hel->name, "frames") ||
            sameString(hel->name, "summary") ||
            sameString(hel->name, "searchTrix") ||
            sameString(hel->name, "html")
            )
            {
            char *urlToData = trackHubRelativeUrl(baseUrl, hel->val);
            if (isNotEmpty(downloadDir))
                {
                dyStringClear(fname);
                char *relName = strrchr(hel->val,'/');
                if (relName != NULL)
                    {
                    relName = relName + 1;
                    dyStringPrintf(fname, "%s%s", downloadDir, relName);
                    }
                else
                    {
                    relName = hel->val;
                    dyStringPrintf(fname, "%s%s", downloadDir, (char *)hel->val);
                    }
                fprintf(out, "%s %s\n", hel->name, relName);
                char *cmd[] = {"wget", "-q", "-O", dyStringContents(fname), urlToData, NULL};

                // use pipelineNoAbort so the loop continues if a url is typo'd or something,
                // but still warn the user
                struct pipeline *pl = pipelineOpen1(cmd, pipelineWrite | pipelineNoAbort, "/dev/null", NULL, 0);
                int ret = pipelineWait(pl);
                if (ret != 0)
                    {
                    warn("wget failed for url: %s", urlToData);
                    }
                }
            else
                fprintf(out, "%s %s\n", hel->name, urlToData);
            }
        else
            fprintf(out, "%s %s\n", hel->name, (char *)hel->val);
        }
    }
fprintf(out, "\n");
hashElFreeList(&helList);
}

void printGenericStanza(struct hash *stanza, FILE *out, char *baseUrl)
/* print a hash to out */
{
struct hashEl *hel, *helList = hashElListHash(stanza);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    fprintf(out, "%s %s\n", hel->name, (char *)hel->val);
    }
fprintf(out,"\n");
}

void printOneFile(char *url, FILE *f, boolean oneFile, char *downloadDir)
/* printOneFile: pass a stanza to appropriate printer */
{
struct lineFile *lf;
struct hash *stanza;
struct hashEl *includeFile;

lf = udcWrapShortLineFile(url, NULL, MAX_HUB_TRACKDB_FILE_SIZE);
while ((stanza = raNextRecord(lf)) != NULL)
    {
    if (hashLookup(stanza, "hub"))
        {
        printHubStanza(stanza, f, url);
        }
    else if (hashLookup(stanza, "genome"))
        {
        printGenomeStanza(stanza, f, url, oneFile);
        }
    else if (hashLookup(stanza, "track"))
        {
        printTrackDbStanza(stanza, f, url, downloadDir);
        }
    else
        {
        // if there's an include file then open and print the include file
        includeFile = hashLookup(stanza, "include");
        if (includeFile != NULL)
            {
            for(; includeFile; includeFile = includeFile->next)
                {
                char *newUrl = trackHubRelativeUrl(url, includeFile->val);
                printOneFile(newUrl, f, oneFile, downloadDir);
                }
            }
        else
            printGenericStanza(stanza, f, url);
        }
    }
lineFileClose(&lf);
freeHash(&stanza);
}

struct trackHub *readHubFromUrl(char *hubUrl)
/* readHubUrl: errCatch around trackHubOpen */
{
struct trackHub *hub = NULL;
struct errCatch *errCatch = errCatchNew();

if (errCatchStart(errCatch))
    hub = trackHubOpen(hubUrl, "");
errCatchEnd(errCatch);
if (errCatch->gotError)
    errAbort("aborting: %s\n", errCatch->message->string);
return hub;
}

FILE *createPathAndFile(char *path)
/* if path contains parent directories that don't exist, create them first before opening file */
{
char *copy = cloneString(path);
if (stringIn("/", copy))
    {
    chopSuffixAt(copy, '/');
    makeDirs(copy);
    // now make the real file
    return mustOpen(path, "w");
    }
return mustOpen(path, "w");
}

void createWriteAndCloseFile(char *fileName, char *url, boolean useOneFile, char *downloadDir)
/* Wrapper around a couple lines */
{
FILE *f;
f = createPathAndFile(fileName);
printOneFile(url, f, useOneFile, downloadDir);
carefulClose(&f);
}

boolean canAccessUrl(char *url)
/* Check if a URL can be accessed by attempting to open it */
{
struct errCatch *errCatch = errCatchNew();
boolean canAccess = FALSE;

if (errCatchStart(errCatch))
    {
    struct lineFile *lf = udcWrapShortLineFile(url, NULL, MAX_HUB_TRACKDB_FILE_SIZE);
    if (lf != NULL)
        {
        canAccess = TRUE;
        lineFileClose(&lf);
        }
    }
errCatchEnd(errCatch);
errCatchFree(&errCatch);
return canAccess;
}

char *parseHubTxtForGenomesFile(char *hubUrl, char **retHubName, boolean *retUseOneFile)
/* Parse hub.txt to get genomesFile path and hub name. Returns genomesFile value or NULL. */
{
struct lineFile *lf;
struct hash *stanza;
char *genomesFile = NULL;
char *hubName = NULL;
boolean useOneFile = FALSE;

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    lf = udcWrapShortLineFile(hubUrl, NULL, MAX_HUB_TRACKDB_FILE_SIZE);
    while ((stanza = raNextRecord(lf)) != NULL)
        {
        if (hashLookup(stanza, "hub"))
            {
            hubName = cloneString(hashFindVal(stanza, "hub"));
            char *gf = hashFindVal(stanza, "genomesFile");
            if (gf != NULL)
                genomesFile = cloneString(gf);
            if (hashFindVal(stanza, "useOneFile"))
                useOneFile = TRUE;
            }
        freeHash(&stanza);
        }
    lineFileClose(&lf);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    errAbort("Error reading hub.txt: %s", errCatch->message->string);
errCatchFree(&errCatch);

if (retHubName)
    *retHubName = hubName;
if (retUseOneFile)
    *retUseOneFile = useOneFile;
return genomesFile;
}

struct simpleGenome *parseGenomesTxt(char *genomesUrl)
/* Parse genomes.txt to get list of genomes with their trackDb paths */
{
struct lineFile *lf;
struct hash *stanza;
struct simpleGenome *genomeList = NULL;

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    lf = udcWrapShortLineFile(genomesUrl, NULL, MAX_HUB_TRACKDB_FILE_SIZE);
    while ((stanza = raNextRecord(lf)) != NULL)
        {
        char *genomeName = hashFindVal(stanza, "genome");
        char *trackDbPath = hashFindVal(stanza, "trackDb");
        if (genomeName != NULL && trackDbPath != NULL)
            {
            struct simpleGenome *sg;
            AllocVar(sg);
            sg->name = cloneString(genomeName);
            sg->trackDbPath = cloneString(trackDbPath);
            slAddHead(&genomeList, sg);
            }
        freeHash(&stanza);
        }
    lineFileClose(&lf);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    errAbort("Error reading genomes.txt: %s", errCatch->message->string);
errCatchFree(&errCatch);

slReverse(&genomeList);
return genomeList;
}

void printGenomesTxtFiltered(char *genomesUrl, char *outputPath, struct hash *skipGenomes)
/* Read genomes.txt and write it out, skipping genomes in skipGenomes hash */
{
struct lineFile *lf;
struct hash *stanza;
FILE *out = createPathAndFile(outputPath);

lf = udcWrapShortLineFile(genomesUrl, NULL, MAX_HUB_TRACKDB_FILE_SIZE);
while ((stanza = raNextRecord(lf)) != NULL)
    {
    char *genomeName = hashFindVal(stanza, "genome");
    if (genomeName != NULL)
        {
        if (hashLookup(skipGenomes, genomeName))
            {
            // Skip this genome stanza
            freeHash(&stanza);
            continue;
            }
        // Print the genome stanza
        printGenomeStanza(stanza, out, genomesUrl, FALSE);
        }
    else
        {
        // Non-genome stanza (shouldn't happen in valid genomes.txt, but handle gracefully)
        printGenericStanza(stanza, out, genomesUrl);
        }
    freeHash(&stanza);
    }
lineFileClose(&lf);
carefulClose(&out);
}

void hubClone(char *hubUrl, boolean download, boolean skipMissingAssemblies)
/* hubClone - Clone the hub text files to a local copy, fixing up bigDataUrls
 * to remote locations if necessary. */
{
struct trackHub *hub;
struct trackHubGenome *genome;
char *hubBasePath, *hubName, *hubFileName;
char *genomesUrl, *genomesDir, *genomesFileName;
char *tdbFileName, *tdbFilePath;
char *path;
FILE *f;
struct dyString *downloadDir = dyStringNew(0);
boolean oneFile = FALSE;

hubBasePath = cloneString(hubUrl);
chopSuffixAt(hubBasePath, '/'); // don't forget to add a "/" back on!
hubFileName = strrchr(hubUrl, '/');
hubFileName += 1;

if (skipMissingAssemblies)
    {
    // Manual parsing mode: don't use trackHubOpen which validates all files upfront
    char *genomesFile = parseHubTxtForGenomesFile(hubUrl, &hubName, &oneFile);
    polishHubName(hubName);

    if (oneFile)
        {
        // For useOneFile hubs, we still need to try trackHubOpen since everything is in one file
        // Fall through to standard processing
        hub = readHubFromUrl(hubUrl);
        if (hub == NULL)
            errAbort("error opening %s", hubUrl);
        makeDirs(hubName);
        path = catTwoStrings(hubName, catTwoStrings("/", hubFileName));
        f = mustOpen(path, "w");
        if (download)
            dyStringPrintf(downloadDir, "%s/", hubName);
        printOneFile(hubUrl, f, oneFile, dyStringContents(downloadDir));
        carefulClose(&f);
        return;
        }

    if (genomesFile == NULL)
        errAbort("No genomesFile found in hub.txt");

    genomesUrl = trackHubRelativeUrl(hubUrl, genomesFile);
    struct simpleGenome *genomeList = parseGenomesTxt(genomesUrl);

    if (genomeList == NULL)
        errAbort("No genomes found in %s", genomesUrl);

    // Write hub.txt
    path = catTwoStrings(hubName, catTwoStrings("/", hubFileName));
    createWriteAndCloseFile(path, hubUrl, FALSE, dyStringContents(downloadDir));

    // Track which genomes to skip
    struct hash *skipGenomes = hashNew(0);

    // Process each genome, checking accessibility
    genomesFileName = catTwoStrings(hubName, catTwoStrings("/", genomesFile));
    char *genomePath = cloneString(genomesFileName);
    chopSuffixAt(genomePath, '/');

    struct simpleGenome *sg;
    for (sg = genomeList; sg != NULL; sg = sg->next)
        {
        char *trackDbUrl = trackHubRelativeUrl(genomesUrl, sg->trackDbPath);
        char *genomeName = sg->name;

        // Strip leading underscore for assembly hubs
        if (startsWith("_", genomeName))
            genomeName = genomeName + 1;

        if (!canAccessUrl(trackDbUrl))
            {
            warn("Skipping assembly '%s': trackDb file not accessible: %s",
                 genomeName, trackDbUrl);
            hashAdd(skipGenomes, sg->name, NULL);
            continue;
            }

        // Make correct directory structure and write trackDb
        genomesDir = catTwoStrings(genomePath, catTwoStrings("/", genomeName));
        if (download)
            {
            dyStringClear(downloadDir);
            dyStringPrintf(downloadDir, "%s/%s/", hubName, genomeName);
            }
        tdbFileName = strrchr(sg->trackDbPath, '/');
        if (tdbFileName != NULL)
            tdbFileName += 1;
        else
            tdbFileName = sg->trackDbPath;
        tdbFilePath = catTwoStrings(genomesDir, catTwoStrings("/", tdbFileName));
        createWriteAndCloseFile(tdbFilePath, trackDbUrl, FALSE, dyStringContents(downloadDir));
        }

    // Write genomes.txt, filtering out skipped genomes
    printGenomesTxtFiltered(genomesUrl, genomesFileName, skipGenomes);

    hashFree(&skipGenomes);
    return;
    }

// Standard mode: use trackHubOpen
hub = readHubFromUrl(hubUrl);
if (hub == NULL)
    errAbort("error opening %s", hubUrl);

hubName = cloneString((char *)hashFindVal(hub->settings, "hub"));
polishHubName(hubName);

if (trackHubSetting(hub, "useOneFile"))
    {
    oneFile = TRUE;
    makeDirs(hubName);
    path = catTwoStrings(hubName, catTwoStrings("/", hubFileName));
    f = mustOpen(path, "w");
    if (download)
        {
        dyStringPrintf(downloadDir, "%s/", hubName);
        }
    printOneFile(hubUrl, f, oneFile, dyStringContents(downloadDir));
    carefulClose(&f);
    }
else
    {
    genome = hub->genomeList;
    if (genome == NULL)
        errAbort("error opening %s file", hub->genomesFile);

    path = catTwoStrings(hubName, catTwoStrings("/", hubFileName));
    createWriteAndCloseFile(path, hubUrl, oneFile, dyStringContents(downloadDir));

    genomesUrl = trackHubRelativeUrl(hub->url, hub->genomesFile);
    genomesFileName = catTwoStrings(hubName, catTwoStrings("/", hub->genomesFile));
    char *genomePath = cloneString(genomesFileName);
    chopSuffixAt(genomePath, '/'); // used later for making the right directory structure

    createWriteAndCloseFile(genomesFileName, genomesUrl, oneFile, dyStringContents(downloadDir));

    for (; genome != NULL; genome = genome->next)
        {
        if (startsWith("_", genome->name)) // assembly hubs have a leading '_'
            genome->name += 1;

        // make correct directory strucutre
        genomesDir = catTwoStrings(genomePath, catTwoStrings("/", genome->name));
        if (download)
            {
            dyStringClear(downloadDir);
            dyStringPrintf(downloadDir, "%s/%s/", hubName, genome->name);
            }
        tdbFileName = strrchr(genome->trackDbFile, '/') + 1;
        tdbFilePath = catTwoStrings(genomesDir, catTwoStrings("/", tdbFileName));
        createWriteAndCloseFile(tdbFilePath, genome->trackDbFile, oneFile, dyStringContents(downloadDir));
        }
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
setUdcCacheDir();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
hubClone(argv[1], optionExists("download"), optionExists("skipMissingAssemblies"));
return 0;
}
