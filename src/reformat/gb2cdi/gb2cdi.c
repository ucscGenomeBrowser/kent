/* gb2cdi - convert from genBank to cDNA info file format. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "wormdna.h"

char *targetSpecies = "Caenorhabditis elegans";
/* 
 Format - text file, one record per line.  | separated fields. 
  accession|side|gene|product|CDS|stage|sex|reserved|Definition-line.
 Example:
  AF004395|3'|full|dbl-1|decapentaplegic protein homolog|109-1206|mixed|hermaphrodite|-|Caenorhabditis elegans germline RNA helicase-4 (glh-4) mRNA, complete cds.
 */

struct cdi
/* Binary form of a cdi record. */
    {
    char *name;
    char *gene;
    char *side;
    char *product;
    int cdsStart, cdsEnd;
    boolean cdsSoftStart, cdsSoftEnd;
    char *stage;
    char *sex;
    char *reserved;
    char *description;
    };

char *threePrime = "-";
char *fivePrime = "+";

int mixedCount = 0;
int maleCount = 0;
int herCount = 0;

int variedCount = 0;
int embryoCount = 0;
int adultCount = 0;
int otherStageCount = 0;

char *safes(char *s)
{
if (s == NULL) s = "?";
return s;
}

void normalizeCdi(struct cdi *cdi)
/* Eliminate synonymes from stage and sex fields. */
{
if (cdi->stage != NULL)
    {
    if (strncmp(cdi->stage, "varied", 6) == 0)
        cdi->stage = "varied";
    else if (strncmp(cdi->stage, "embryo", 6) == 0)
        cdi->stage = "embryo";
    else if (strstr(cdi->stage, "mix"))
        cdi->stage = "varied";
    else if (strstr(cdi->stage, "nstaged"))
        cdi->stage = "varied";
    else if (strstr(cdi->stage, "Mix"))
        cdi->stage = "varied";
    else if (strstr(cdi->stage, "mbryo"))
        cdi->stage = "embryo";
    else if (strstr(cdi->stage, "dult"))
        cdi->stage = "adult";
    if (sameString(cdi->stage, "adult"))
        ++adultCount;
    else if (sameString(cdi->stage, "embryo"))
        ++embryoCount;
    else if (sameString(cdi->stage, "varied"))
        ++variedCount;
    else
        {
        ++otherStageCount;
        printf("Unusual stage %s in %s (%s) (%s) %s\n",
            cdi->stage, cdi->name, safes(cdi->gene), safes(cdi->product), safes(cdi->description));
        }
    }
if (cdi->sex != NULL)
    {
    boolean gotMale = FALSE;
    boolean gotHer = FALSE;
    char *sex = cdi->sex;
    if (strstr(sex, "male") || strstr(sex, "Male"))
        gotMale = TRUE;
    if (strstr(sex, "herm") || strstr(sex, "Herm"))
        gotHer = TRUE;
    if (gotHer && gotMale)
        {
        ++mixedCount;
        cdi->sex = "mixed";
        }
    else if (gotHer)
        {
        ++herCount;
        cdi->sex = "herm";
        }
    else if (gotMale)
        {
        ++maleCount;
        cdi->sex = "male";
        }
    }
}

void writeCdiField(FILE *faFile, FILE *cdiFile, char *field)
/* Write one field of cdi file. */
{
if (field == NULL)
    field = "?";
fputs(field, faFile);
fputs(field, cdiFile);
fputc('|', faFile);
fputc('|', cdiFile);
}

static int totalCount = 0;

void writeCdi(struct cdi *cdi, FILE *faFile, FILE *cdiFile)
/* Write one record of cdi file. */
{
if (++totalCount % 1000 == 0)
    printf("cDNA # %d\n", totalCount);
normalizeCdi(cdi);
fprintf(faFile, ">%s ", cdi->name);
fprintf(cdiFile, ">%s ", cdi->name);
writeCdiField(faFile, cdiFile, cdi->side);
writeCdiField(faFile, cdiFile, cdi->gene);
writeCdiField(faFile, cdiFile, cdi->product);
if (cdi->cdsStart != 0 || cdi->cdsEnd != 0)
    {
    fprintf(faFile, "%s%d..%s%d|", 
        (cdi->cdsSoftStart ? "<" : ""),cdi->cdsStart, 
        (cdi->cdsSoftEnd ? ">" : ""), cdi->cdsEnd);
    fprintf(cdiFile, "%s%d..%s%d|", 
        (cdi->cdsSoftStart ? "<" : ""),cdi->cdsStart, 
        (cdi->cdsSoftEnd ? ">" : ""), cdi->cdsEnd);
    }
else
    writeCdiField(faFile, cdiFile, NULL);
writeCdiField(faFile, cdiFile, cdi->stage);
writeCdiField(faFile, cdiFile, cdi->sex);
writeCdiField(faFile, cdiFile, cdi->reserved);
writeCdiField(faFile, cdiFile, cdi->description);
fputc('\n',faFile);
fputc('\n',cdiFile);
}

void findSubKeys(struct lineFile *lf, char **keys, char **dests, 
    int *destSizes, char **retDest, int keyCount)
{
int size;
char *line;
int lineSize;
int i;

for (i=0; i<keyCount; ++i)
    retDest[i] = NULL;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (!isspace(line[0]) || !isspace(line[5]))
        {
        lineFileReuse(lf);
        return;
        }
    line = skipLeadingSpaces(line);
    for (i=0; i<keyCount; ++i)
        {
        char *key = keys[i];
        int keySize = strlen(key);
        char *dest = dests[i];
        if (strncmp(line, key, keySize) == 0)
            {
            line += keySize;
            /* Remove surrounding spaces and quotes. */
            eraseTrailingSpaces(line);
            if (line[0] == '"')
                line += 1;
            size = strlen(line);
            if (line[size-1] == '"')
                line[size-1] = 0;
            strncpy(dest, line, destSizes[i]);
            retDest[i] = dest;
            break;
            }
        }
    }
}

char *findOneSubKey(struct lineFile *lf, char *key, char *dest, int destSize)
{
char *retDest;

findSubKeys(lf, &key, &dest, &destSize, &retDest, 1);
return retDest; 
}

int koharaCount = 0;
int martinCount = 0;
int uaWuEstCount = 0;
int stratageneCount = 0;
int fullCount = 0;

char *nameAfterClone(char **words, int wordCount, char *name, char *retSide)
/* The name is the word after "clone", and is followed by 3' or 5' */
{
int cloneIx;

for (cloneIx = 0; cloneIx < wordCount; ++cloneIx)
    {
    if (sameString(words[cloneIx], "clone"))
        {
        if (cloneIx < wordCount+1)
            {
            sprintf(name, "%s.%c", words[cloneIx+1], words[cloneIx+2][0]);
            *retSide = words[cloneIx+2][0];
            return name;
            }
        }
    }
return NULL;
}

char *lfNextLine(struct lineFile *lf)
/* Get next line from linefile. */
{
int lineSize;
char *line = NULL;
lineFileNext(lf, &line, &lineSize);
return line;
}

void procOne(char *inName, FILE *faFile, FILE *cdiFile, struct hash *cdnaNameHash)
/* Append contents of one genBank file to cdi file. */
{
char *line;
char *origLine = needMem(16*1024);
char *words[256];
int wordCount;
struct lineFile *lf = lineFileOpen(inName, TRUE);
int maxDescription = 16*1024 - 1;
char *description = needMem(maxDescription+1);
char cdiName[64];
char geneName[64];
static char sex[64];
static char devStage[128];
int maxProduct = 2048-1;
char *product = needMem(maxProduct+1);
struct cdi *cdi = NULL;
boolean isStratagene;  /* Stratagene EST? */
boolean isUaWuEst;     /* Univ. of Arizona/Washington U. EST? */
char *section;
int baseCount;
DNA *dna = NULL;
int dnaAllocated = 0;
int lineSize;
int locusCount = 0;

printf("Processing %s\n", inName);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (!isspace(line[0]))
        {
        wordCount = chopLine(line, words);
        section = words[0];
        if (sameString(section, "LOCUS"))
            {
	    ++locusCount;
	    if ((locusCount & 0x3ff) == 0)
		{
	        printf(".");
		fflush(stdout);
		}
            if (cdi != NULL)
                {
                errAbort("LOCUS in the middle of another line %d of %s\n",
                    lf->lineIx, inName);
                }
            if (wordCount < 6)
                errAbort("Short LOCUS line %d of %s\n", lf->lineIx, inName);
            baseCount = atol(words[2]);
            if (baseCount <= 0)
                {
                errAbort("Expecting positive number of base pairs, got %s line %d of %s",
                    lf->lineIx, inName, words[2]);
                }
            if (sameString(words[4], "mRNA"))
                AllocVar(cdi);
            } 
        else if (cdi && sameString(section, "ACCESSION"))
            {
            if (cdi->name == NULL)
                {
                strncpy(cdiName, words[1], sizeof(cdiName));
                cdi->name = cdiName;
                }
            }
        else if (cdi && sameString(section, "DEFINITION"))
            {
            char side = 0;
            isStratagene = FALSE;
            isUaWuEst = FALSE;
            if (sameString(words[2], "Yuji") && sameString(words[3], "Kohara"))
                {
                /* One of Kohara's clones, make up a name for it, and generally
                 * otherwise ignore definition - all 70,000 say almost the same thing. */
                ++koharaCount;
                line = lfNextLine(lf);
                wordCount = chopLine(line, words);
                if ((cdi->name = nameAfterClone(words, wordCount, cdiName, &side)) == NULL)
                    errAbort("Unusual Kohara line %d of %s!",lf->lineIx, lf->fileName);
                }
            else if (sameString(words[2], "Chris") && sameString(words[3], "Martin") && sameString(words[4], "sorted"))  
                
                {
                char uglyfLine[1024];
                ++martinCount;
                cdi->stage = "varied";
                cdi->sex = "hermaphrodite";
                line = lfNextLine(lf);
                strcpy(uglyfLine, line);
                wordCount = chopLine(line, words);
                if ((cdi->name = nameAfterClone(words, wordCount, cdiName, &side)) == NULL)
                    errAbort("Unusual Martin line %d of %s!",lf->lineIx, lf->fileName);
                }
            else if (sameString(words[4], "Stratagene") && sameString(words[5], "(cat.") )
                {
                char *catNo = words[6] + 1;
                int len;
                ++stratageneCount;
                isStratagene = TRUE;
                if (sameString(words[2], "Mixed"))
                    cdi->stage = "varied";
                else if (sameString(words[3], "embryo,"))
                    cdi->stage = "embryo";
                else
                    errAbort("Unusual Stratagene stage in line %d of %s", lf->lineIx, lf->fileName);
                line = lfNextLine(lf);
                wordCount = chopLine(line, words);
                if (wordCount < 6 || differentString(words[2], "clone") )
                    errAbort("Unusual Stratagene line %d of %s", lf->lineIx, lf->fileName);
                sprintf(cdiName, "%s", words[3]);
                cdi->name = cdiName;
                /* Chop of occassional trailing comma. */
                len = strlen(cdiName);
                if (cdiName[len-1] == ',')
                    cdiName[len-1] = 0;
                }
            else
                {
                /* Normal case definition processing - chop up multi-line description by words,
                 * and put words all together back on one long line. */
                int wordLen, newLen;
                int descriptionLen = 0;
                int startWord = 1;
                char *word;
                int i;


                side = '5';

                /* Do special processing for Univ. of Ariz/Wash U. ESTs. */
                if (wordCount > 1)
                    {
                    word = words[1];
                    if (word[0] == 'c' && word[1] == 'a')
                        {
                        char c;
                        wordLen = strlen(word);
                        c = word[wordLen-2];
                        if (word[wordLen-1] == '1' && word[wordLen-3] == '.' && (c == 'x' || c == 'y'))
                            {
                            isUaWuEst = TRUE;
                            if (strncmp(words[wordCount-1], "5'", 2) == 0)
                                side = '5';
                            else if (strncmp(words[wordCount-1], "3'", 2) == 0)
                                side = '3';
                            else
                                {
                                errAbort("Unusual Univ. of Ariz/Wash U. EST line %d of %s",
                                    lf->lineIx,  lf->fileName);
                                }
                            strcpy(cdiName, word);
                            cdiName[wordLen-2] = side;
                            cdiName[wordLen-1] = 0;
                            cdi->name = cdiName;
                            }
                        }
                    }
                if (isUaWuEst)
                    ++uaWuEstCount;
                else
                    ++fullCount;
                for (;;)
                    {
                    for (i=startWord; i<wordCount; ++i)
                        {
                        word = words[i];
                        wordLen = strlen(word);
                        newLen = wordLen + descriptionLen + 1;
                        if (newLen > maxDescription)
                            {
                            errAbort("Long description line %d of %s\n", lf->lineIx, lf->fileName);
                            break;
                            }
                        memcpy(description+descriptionLen, word, wordLen);
                        description[newLen-1] = ' ';
                        descriptionLen = newLen;
                        }
                    startWord = 0;
                    line = lfNextLine(lf);
                    if (line == NULL)
                        {
                        errAbort("EOF in DESCRIPTION");
                        }
                    if (!isspace(line[0]))
                        {
                        lineFileReuse(lf);
                        break;
                        }
                    wordCount = chopLine(line, words);
                    }
                description[descriptionLen] = 0;
                cdi->description = description;
                }
            if (side == '3')
                cdi->side = threePrime;
            else if (side == '5')
                cdi->side = fivePrime;
            }
        else if (cdi && isStratagene && sameString(section, "COMMENT"))
            {
            char *dir;
            char dirBuf[24];
            dir = findOneSubKey(lf, "Seq primer: M13 ", dirBuf, sizeof(dirBuf));
            if (dir)
                {
                if (dir[0] == 'F')
                    {
                    cdi->side= threePrime;
                    strcat(cdi->name, ".3");
                    }
                else if (dir[0] == 'R')
                    {
                    cdi->side = fivePrime;
                    strcat(cdi->name, ".5");
                    }
                }
            if (cdi->side == NULL)
                errAbort("Stratagene without direction line %d of %s", lf->lineIx, lf->fileName);
            }
        else if (cdi && sameString(section, "FEATURES"))
            {
            for (;;)
                {
                line = lfNextLine(lf);
                if (!isspace(line[0]))
                    {
                    lineFileReuse(lf);
                    break;
                    }
                strcpy(origLine, line);
                chopLine(line, words);
                section = words[0];
                if (cdi->sex == NULL && cdi->stage == NULL && sameString(section, "source"))
                    {
                    static char *keys[2] = {"/dev_stage=", "/sex="};
                    static char *dests[2] = { devStage, sex, };
                    static int destSizes[2] = {sizeof(devStage), sizeof(sex)};
                    char *retDests[2];
                    findSubKeys(lf, keys, dests, destSizes, retDests, 2);
                    cdi->stage = retDests[0];
                    cdi->sex = retDests[1];
                    }
                else if (sameString(section, "gene"))
                    {
                    cdi->gene = findOneSubKey(lf, "/gene=", geneName, sizeof(geneName));
                    }
                else if (sameString(section, "CDS"))
                    {
                    char *parts[2];
                    int partCount;
                    if (wordCount < 2)
                        errAbort("Short CDS line %d of %s", lf->lineIx, lf->fileName);
                    partCount = chopString(words[1], ".", parts, ArraySize(parts));
                    if (partCount != 2)
                        errAbort("Strange CDS line %d of %s", lf->lineIx, lf->fileName);
                    if (parts[0][0] == '<')
                        {
                        cdi->cdsSoftStart = TRUE;
                        parts[0] += 1;
                        }
                    if (!isdigit(parts[0][0]))
                        {
                        warn("Expecting number at start of CDS got %s line %d of %s",
                            parts[0], lf->lineIx, lf->fileName);
                        }
                    else
                        {
                        cdi->cdsStart = atoi(parts[0]);
                        if (parts[1][0] == '>')
                            {
                            cdi->cdsSoftEnd = TRUE;
                            parts[1] += 1;
                            }
                        if (!isdigit(parts[1][0]))
                            {
                            errAbort("Expecting number at end of CDS got %s line %d of %s",
                                parts[0], lf->lineIx, lf->fileName);
                            }
                        cdi->cdsEnd = atoi(parts[1]);
                        cdi->product = findOneSubKey(lf, "/product=", product, maxProduct);
                        }
                    }
                }
            }
        else if (cdi && sameString(section, "ORIGIN"))
            {
            int dnaCount = 0;
            char c;
            int i;
            DNA base;
            int lineSize = 0;

            if (hashLookup(cdnaNameHash, cdi->name))
                {
                warn("Duplicate cDNA name %s.  Ignoring all but first. Line %d of %s",
                    cdi->name, lf->lineIx, lf->fileName);
                }
            else
                {
                hashAdd(cdnaNameHash, cdi->name, NULL);
                writeCdi(cdi, faFile, cdiFile);
                if (dnaAllocated < baseCount)
                    {
                    dnaAllocated = 2*baseCount;
                    freeMem(dna);
                    dna = needLargeMem(dnaAllocated);
                    }
                /* Loop to read DNA */
                for (;;)
                    {
                    line = lfNextLine(lf);
                    if (!isspace(line[0]))
                        {
                        lineFileReuse(lf);
                        break;
                        }
                    /* Add in this line's worth of DNA. */
                    while ((c = *line++) != 0)
                        {
                        if ((base = ntChars[c]) != 0)
                            {
                            if (dnaCount >= dnaAllocated)
                                {
                                errAbort("Got more than %d bases, header said there "
                                         "should only be %d line %d of %s!",
                                         dnaCount, baseCount, lf->lineIx, lf->fileName);
                                }
                            dna[dnaCount++] = base;                
                            }
                        }
                    }
                /* Now write out in .fa format. */
                for (i=0; i<dnaCount; i += lineSize)
                    {
                    lineSize = dnaCount - i;
                    if (lineSize > 50) lineSize = 50;
                    mustWrite(faFile, dna+i, lineSize);
                    putc('\n', faFile);
                    }
                }
            freez(&cdi);
            }
        }
    else
        {
	if (cdi != NULL && startsWith("  ORGANISM ", line))
	    {
	    line += strlen("  ORGANISM ");
	    line = trimSpaces(line);
	    if (!sameString(targetSpecies, line))
	        freez(&cdi);
	    }
	}
    }
if (cdi != NULL)
    {
    errAbort("Unterminated last line");
    }
freeMem(dna);
freeMem(origLine);
freeMem(product);
freeMem(description);
lineFileClose(&lf);
}


int main(int argc, char *argv[])
{
char *inName, *faName, *cdiName;
FILE *faFile, *cdiFile;
int faIx;
int i;
struct hash *cdnaNameHash = NULL;

if (argc < 4)
    {
    errAbort("gb2cdi - convert GeneBank (GB) files to .fa and cDna Info (CDI) file.\n"
             "usage:\n"
             "    gb2cdi file(s).gb file.fa file.cdiFile");
    }
dnaUtilOpen();
faIx = argc-2;
faName = argv[faIx];
cdiName = argv[faIx+1];
faFile = mustOpen(faName, "w");
cdiFile = mustOpen(cdiName, "w");
cdnaNameHash = newHash(14);
for (i=1; i<faIx; ++i)
    {
    inName = argv[i];
    procOne(inName, faFile, cdiFile, cdnaNameHash);
    }
printf("%d by Kohara, %d by Martin, %d by Stratagene, %d Univ Ariz/U Wash, %d full cDNAs\n",
    koharaCount, martinCount, stratageneCount, uaWuEstCount, fullCount);
printf("%d male %d hermaphrodite %d mixed\n", maleCount, herCount, mixedCount);
printf("%d adult %d embryo %d varied %d other\n", adultCount, embryoCount, variedCount, otherStageCount);
printf("%d cDNAs in all\n", totalCount);

fclose(faFile);
fclose(cdiFile);
return 0;
}
