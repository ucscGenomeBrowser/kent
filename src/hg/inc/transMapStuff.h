/* transMapStuff - common definitions and functions for supporting transMap
 * tracks in the browser CGIs */
#ifndef transMapStuff_h
#define transMapStuff_h
struct trackDb;

/* 
 * transMap table names setting names in trackDb. To get tables shared between
 * genomes, use names "hgFixed.transMapSrc"
 */
#define transMapInfoTblSetting    "transMapInfo"
#define transMapSrcTblSetting     "transMapSrc"
#define transMapGeneTblSetting    "transMapGene"

char *transMapIdToAcc(char *id);
/* remove all unique suffixes (starting with last `-') from any TransMap 
 * id.  WARNING: static return */
#endif
