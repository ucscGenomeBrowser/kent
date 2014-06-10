/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>


#include "common.h"


typedef struct bgPoint {
    long  position;
    double score;
    long  number;
    long  radius;
} bgPoint;

/* finds the smallest point between the first and last iterators whoes position 
 * is grather than or equal to position */
bgPoint* findUpperBound(long position, bgPoint* first, long length) {
    int half;
    bgPoint* middle;

    while(length > 0) {
        half = length / 2;
        middle = first + half;

        if(middle->position < position) {
            first = middle + 1;
            length = length - half - 1;
        } else {
            length = half;
        }
    }

    return first;
}

/* print a simple usage message */
void usage(char* programName) {
    char dir[256];
    char name[128];
    char extension[64];
    
    splitPath(programName, dir, name, extension);
    fprintf(stderr, "usage: %s backgroundFile windowsFile defaultBackgroundSize defaultBackgroundPercentID [semi]\n", name);
}

/* load the background from the given filename and put the numnber of windows
 * at the given address */
bgPoint* loadBackground(char* filename, long* numberOfWindows) {
    char chrom[16];
    char c[16];
    long chromStart;
    long chromEnd;
    long number;
    long AA;
    long AC;
    long AG;
    long AT;
    long CA;
    long CC;
    long CG;
    long CT;
    long GA;
    long GC;
    long GG;
    long GT;
    long TA;
    long TC;
    long TG;
    long TT;

    FILE* backgroundFile;
    bgPoint* backgroundData = 0;
    long i;

    *numberOfWindows = 0;
    backgroundFile = mustOpen(filename, "r");

    /* see if the first character is a # */
    *c = fgetc(backgroundFile);
    if(*c == '#') {
        /* read the rest of the line */
        while(fgetc(backgroundFile) != '\n')
            ;
    } else
        ungetc(*c, backgroundFile);
        
    /* count now many windows there are */
    while(!feof(backgroundFile)) {
        if(fscanf(backgroundFile, "%15s\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t"
                                  "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld",
                c, &chromStart, &chromEnd, &number, 
                &AA, &AC, &AG, &AT, &CA, &CC, &CG, &CT,
                &GA, &GC, &GG, &GT, &TA, &TC, &TG, &TT) == 20)
            (*numberOfWindows)++;
    }
    
    /* add two to account for the end and begin sentries */
    (*numberOfWindows) += 2;

    backgroundData = needLargeMem((*numberOfWindows) * sizeof(bgPoint));
    
    /* now read the data starting from the begining of the file */
    rewind(backgroundFile); 

    /* see if the first character is a # */
    *c = fgetc(backgroundFile);
    if(*c == '#') {
        /* read the rest of the line */
        while(fgetc(backgroundFile) != '\n')
            ;
    } else
        ungetc(*c, backgroundFile);
    
    /* added a begin of list sentry */
    backgroundData[0].position = -1;
    backgroundData[0].score = -1;
    backgroundData[0].number = -1;
    backgroundData[0].radius = -1;

    /* read the first data file, and store the chrom to make sure that they
     * are the same for all windows */
    assert(fscanf(backgroundFile, "%15s\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t"
                                  "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld",
                chrom, &chromStart, &chromEnd, &number, 
                &AA, &AC, &AG, &AT, &CA, &CC, &CG, &CT,
                &GA, &GC, &GG, &GT, &TA, &TC, &TG, &TT) == 20);
    backgroundData[1].position = (chromStart + chromEnd) / 2;
    backgroundData[1].score = (((double)AA) + CC + GG +TT) /
        (((double)AA) + AC + AG + AT + CA + CC + CG + CT + GA + GC + GG + GT + TA + TC + TG + TT);
    backgroundData[1].number = number;
    backgroundData[1].radius = (chromEnd - chromStart) / 2;

    for(i = 2; i < *numberOfWindows - 1; i++) {
        assert(fscanf(backgroundFile, "%15s\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t"
                               "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld",
                c, &chromStart, &chromEnd, &number, 
                &AA, &AC, &AG, &AT, &CA, &CC, &CG, &CT,
                &GA, &GC, &GG, &GT, &TA, &TC, &TG, &TT) == 20);

        /* make sure that all the windows are on the smae chrom */
        if(!sameString(chrom, c))
            errAbort("all window do not come from the same chromosome "
                    "in file %s\n", filename);

        backgroundData[i].position = (chromStart + chromEnd) / 2;
        backgroundData[i].score = (((double)AA) + CC + GG +TT) /
            (((double)AA) + AC + AG + AT + CA + CC + CG + CT + GA + GC + GG + GT + TA + TC + TG + TT);
        backgroundData[i].number = number;
        backgroundData[i].radius = (chromEnd - chromStart) / 2;
    }

    /* added an end of list sentry */
    backgroundData[i].position = LONG_MAX;
    backgroundData[i].score = -1;
    backgroundData[i].number = -1;
    backgroundData[i].radius = -1;
    
    fclose(backgroundFile);

    return backgroundData;
}

/* get the largest radius from the given array of background points */
long getMaxRadius(bgPoint* backgroundData, long numberOfWindows) {
    long maxRadius = 0;

    long i;

    for(i = 0; i < numberOfWindows; i++) {
        if(backgroundData[i].radius > maxRadius)
            maxRadius = backgroundData[i].radius;
    }

    return maxRadius;
}

/* find and return the score of the window in the backgroundData that encloses
 * and is nearest to position */
bgPoint* getNearestEnclosing(long position, bgPoint* backgroundData, long numberOfWindows,
        long maxRadius) {

    bgPoint* current;
    long minDistance = LONG_MAX;
    bgPoint* minWindow = 0;

    current = findUpperBound(position + maxRadius, backgroundData, numberOfWindows);

    while(current->position >= position - maxRadius && current >= backgroundData) {
        /* if the current window contains the point */
        if(current->position - current->radius <= position &&
                position <= current->position + current->radius) {

            /* check to see if it is closer than the current minimum */
            if(labs(current->position - position) < minDistance) {
                minDistance = labs(current->position - position);
                minWindow = current;
            }
        }

        --current;
    }

    return minWindow;
}

int bgPointCompar(const void* left, const void* right) {
    if(((bgPoint*)left)->position < ((bgPoint*)right)->position)
        return -1;
    else if(((bgPoint*)left)->position < ((bgPoint*)right)->position)
        return 0;
    else
        return 1;
}

int main(int argc, char* argv[]) {
    bgPoint* backgroundData;
    long numberOfWindows;
    long maxRadius;
    bgPoint* background;

    FILE* windowsFile;
    char chrom[16];
    long chromStart;
    long chromEnd;
    double percentId;
    long number;

    double backgroundPercentId;
    long backgroundNumber;
    double score;

    long AA;
    long AC;
    long AG;
    long AT;
    long CA;
    long CC;
    long CG;
    long CT;
    long GA;
    long GC;
    long GG;
    long GT;
    long TA;
    long TC;
    long TG;
    long TT;
    
    long globalNumber;
    double globalPercentId;

    long numWindowsScored = 0;
    long numWindowsDefaulted = 0;
    
    /* make sure that there are the correct number of arguments */
    if(argc < 5 || argc > 6) {
        usage(argv[0]);
        exit(10);
    }

    /* load the data and get the number of windows */
    backgroundData = loadBackground(argv[1], &numberOfWindows);
    maxRadius = 200 * getMaxRadius(backgroundData, numberOfWindows);
    qsort(backgroundData, numberOfWindows, sizeof(bgPoint), bgPointCompar);

    globalNumber = atol(argv[3]); // 100516125;
    globalPercentId = atof(argv[4]); // 0.668947295769709;

    /*fprintf(stderr, "maxRadius: %ld\n", maxRadius);*/
    fprintf(stderr, "loaded %ld background window\n", numberOfWindows - 2);

    windowsFile = mustOpen(argv[2], "r");
    /* see if the first character is a # */
    *chrom = fgetc(windowsFile);
    if(*chrom == '#') {
        /* read the rest of the line */
        while(fgetc(windowsFile) != '\n')
            ;
    } else
        ungetc(*chrom, windowsFile);
    
    while(!feof(windowsFile)) {
        if(fscanf(windowsFile, "%15s\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t"
                               "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld",
                chrom, &chromStart, &chromEnd, &number, 
                &AA, &AC, &AG, &AT, &CA, &CC, &CG, &CT,
                &GA, &GC, &GG, &GT, &TA, &TC, &TG, &TT) != 20)
            continue;

        percentId = (((double)AA) + CC + GG +TT) /
            (((double)AA) + AC + AG + AT + CA + CC + CG + CT + GA + GC + GG + GT + TA + TC + TG + TT);

        background = getNearestEnclosing( (chromStart + chromEnd) / 2, backgroundData,
            numberOfWindows, maxRadius);            

        /* if we didn't find an enclosing window */
        if(background == 0 || background->number == -1) {
            /* used the genome-wide background numbers */
            backgroundPercentId = globalPercentId;
            backgroundNumber = globalNumber;

            numWindowsDefaulted++;
        } else {
            backgroundPercentId = background->score;
            backgroundNumber = background->number;
        }

        /* adjust for the percent identity for the current window */
        backgroundPercentId = (backgroundPercentId * backgroundNumber - number * percentId) /
            (backgroundNumber - number);
        
        if(argc == 5)
            score = number * (percentId - backgroundPercentId) / sqrt(number * backgroundPercentId * (1.0 - backgroundPercentId));
        else
            score = number * (percentId - backgroundPercentId) / sqrt(number);
        printf("%s\t%ld\t%ld\t%lf\t%ld\n", chrom, chromStart, chromEnd, score, number);

        numWindowsScored++;
    }
    
    fprintf(stderr, "%ld windows of the %ld total windows scored used the default background\n", numWindowsDefaulted, numWindowsScored);
    
    return 0;
}
