#ifndef ENCODE_H
#define ENCODE_H

char *encodeErgeName(struct track *tg, void *item);
/* return the actual data name, in form xx/yyyy cut off xx/ return yyyy */
  
void encodeErgeMethods(struct track *tg);
/* setup special methods for ENCODE dbERGE II tracks */

Color encodeStanfordNRSFColor(struct track *tg, void *item, struct hvGfx *hvg);
/* different color for negative strand */

void encodeStanfordNRSFMethods(struct track *tg);
/* custom methods for ENCODE Stanford NRSF data */

void loadEncodeRna(struct track *tg);
/* Load up encodeRna from database table to track items. */

void freeEncodeRna(struct track *tg);
/* Free up encodeRna items. */

Color encodeRnaColor(struct track *tg, void *item, struct hvGfx *hvg);
/* Return color of encodeRna track item. */

char *encodeRnaName(struct track *tg, void *item);
/* Return RNA gene name. */

void encodeRnaMethods(struct track *tg);
/* Make track for rna genes . */

void encodePeakMethods(struct track *tg);
/* Methods for ENCODE peak track uses mostly linkedFeatures. */

void encodePeakMethodsCt(struct track *tg);
/* Methods for ENCODE peak track uses mostly linkedFeatures. */

#endif /* ENCODE_H */
