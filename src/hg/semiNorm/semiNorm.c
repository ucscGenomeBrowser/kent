#include <stdio.h>
#include <stdlib.h>

#include "common.h"

const long GLOBAL_NUMBER = 97210242;
const float GLOBAL_PERCENT_ID = 0.669295515178329;

typedef struct bgPoint {
    long position;
    float score;
    int number;
    long radius;
} bgPoint;

bgPoint background[] = {
    { -10, 1.0, 10,  1},
    {  10, 1.0, 10, 10},
    {  35, 4.0, 10, 15},
    { 666, 4.0, 10,  1}
};


/* finds the smallest point between the first and last iterators whoes position 
 * is grather than or equal to position */
bgPoint* findUpperBound(long position, bgPoint* first, int length) {
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
    printf("usage: %s backgroundFile windowsFile\n", name);
}

/* load the background from the given filename and put the numnber of windows
 * at the given address */
bgPoint* loadBackground(char* filename, long* numberOfWindows) {
    char chrom[16];
    char c[16];
    long chromStart;
    long chromEnd;
    float percentId;
    long number;
    long invalid;

    FILE* backgroundFile;
    bgPoint* backgroundData = 0;
    long i;

    *numberOfWindows = 0;
    backgroundFile = mustOpen(filename, "r");

    /* count now many windows there are */
    while(!feof(backgroundFile)) {
        if(fscanf(backgroundFile, "%15s\t%ld\t%ld\t%f\t%ld\t%ld",
                c, &chromStart, &chromEnd, &percentId, &number, &invalid) == 6)
            (*numberOfWindows)++;
    }

    backgroundData = needLargeMem((*numberOfWindows) * sizeof(bgPoint));
    
    /* now read the data starting from the begining of the file */
    rewind(backgroundFile); 
    
    /* read the first data file, and store the chrom to make sure that they
     * are the same for all windows */
    fscanf(backgroundFile, "%10s\t%ld\t%ld\t%f\t%ld\t%ld",
            chrom, &chromStart, &chromEnd, &percentId, &number, &invalid);
    backgroundData[0].position = (chromStart + chromEnd) / 2;
    backgroundData[0].score = percentId;
    backgroundData[0].number = number;
    backgroundData[0].radius = (chromEnd - chromStart) / 2;

    for(i = 1; i < *numberOfWindows; i++) {
        fscanf(backgroundFile, "%10s\t%ld\t%ld\t%f\t%ld\t%ld",
                c, &chromStart, &chromEnd, &percentId, &number, &invalid);

        /* make sure that all the windows are on the smae chrom */
        if(!sameString(chrom, c))
            errAbort("all window do not come from the same chromosome "
                    "in file %s\n", filename);

        backgroundData[i].position = (chromStart + chromEnd) / 2;
        backgroundData[i].score = percentId;
        backgroundData[i].number = number;
        backgroundData[i].radius = (chromEnd - chromStart) / 2;
    }
    
    fclose(backgroundFile);

    return backgroundData;
}

/* get the largest radius from the given array of background points */
long getMaxRadius(bgPoint* backgroundData, long numberOfWindows) {
    int maxRadius = 0;

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
    int minDistance = maxRadius + 1;
    bgPoint* minWindow = 0;

    current = findUpperBound(position + maxRadius, backgroundData, numberOfWindows);

    while(current->position > position - maxRadius && current != backgroundData - 1) {
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

int main(int argc, char* argv[]) {
    bgPoint* backgroundData;
    long numberOfWindows;
    long maxRadius;
    bgPoint* background;

    FILE* windowsFile;
    char chrom[16];
    long chromStart;
    long chromEnd;
    float percentId;
    long number;
    long invalid;

    float backgroundPercentId;
    long backgroundNumber;
    float score;
    
    /* make sure that there are the correct number of arguments */
    if(argc != 3) {
        usage(argv[0]);
        exit(10);
    }

    /* load the data and get the number of windows */
    backgroundData = loadBackground(argv[1], &numberOfWindows);
    maxRadius = 200 * getMaxRadius(backgroundData, numberOfWindows);

    fprintf(stderr, "maxRadius: %ld\n", maxRadius);

    windowsFile = mustOpen(argv[2], "r");
    while(!feof(windowsFile)) {
        if(fscanf(windowsFile, "%15s\t%ld\t%ld\t%f\t%ld\t%ld",
                chrom, &chromStart, &chromEnd, &percentId, &number, &invalid) == 6) {

            background = getNearestEnclosing( (chromStart + chromEnd) / 2, backgroundData,
                numberOfWindows, maxRadius);            

            if(background == 0) {
                backgroundPercentId = GLOBAL_PERCENT_ID;
                backgroundNumber = GLOBAL_NUMBER;
            } else {
                backgroundPercentId = background->score;
                backgroundNumber = background->number;
            }

            /* adjust for the percent identity for the current window */
            backgroundPercentId = (backgroundPercentId * backgroundNumber - number * percentId) /
                (backgroundNumber - number);
            
            score = number * (percentId - backgroundPercentId) / sqrt(number);
            printf("%s\t%ld\t%ld\t%f\t%ld\n", chrom, chromStart, chromEnd, score, number);
        }
    }
    
    return 0;
}
