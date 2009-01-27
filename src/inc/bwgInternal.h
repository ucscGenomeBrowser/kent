/* bwgInternal - stuff to create and use bigWig files.  Generally you'll want to use the
 * simpler interfaces in the bigWig module instead.  This file is good reading though
 * if you want to extend the bigWig interface, or work with bigWig files directly
 * without going through the Kent library. */

#ifndef BIGWIGFILE_H
#define BIGWIGFILE_H

/* bigWig file structure:
 *     fixedWidthHeader
 *         magic# 		4 bytes
 *	   zoomLevels		4 bytes
 *         chromosomeTreeOffset	8 bytes
 *         fullDataOffset	8 bytes
 *	   fullIndexOffset	8 bytes
 *         reserved            32 bytes
 *     zoomHeaders		there are zoomLevels number of these
 *         reductionLevel	4 bytes
 *	   reserved		4 bytes
 *	   dataOffset		8 bytes
 *         indexOffset          8 bytes
 *     chromosome b+ tree       bPlusTree index
 *     full data
 *         sectionCount		4 bytes
 *         section data		section count sections, of three types
 *     full index               ciTree index
 *     zoom info             one of these for each zoom level
 *         zoom data
 *             zoomCount	4 bytes
 *             zoom data	there are zoomCount of these items
 *                 chromId	4 bytes
 *	           chromStart	4 bytes
 *                 chromEnd     4 bytes
 *                 validCount	4 bytes
 *                 minVal       4 bytes float 
 *                 maxVal       4 bytes float
 *                 sumData      4 bytes float
 *                 sumSquares   4 bytes float
 *         zoom index        	ciTree index
 */

enum bwgSectionType 
/* Code to indicate section type. */
    {
    bwgTypeBedGraph=1,
    bwgTypeVariableStep=2,
    bwgTypeFixedStep=3,
    };

struct bigWigFile *bigWigFileOpen(char *fileName);
/* Open up a big wig file. */

void bigWigFileClose(struct bigWigFile **pBwf);
/* Close down a big wig file. */


#endif /* BIGWIGFILE_H */
