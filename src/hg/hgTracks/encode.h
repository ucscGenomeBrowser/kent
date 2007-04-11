#ifndef ENCODE_H
#define ENCODE_H

char *encodeErgeName(struct track *tg, void *item);
/* return the actual data name, in form xx/yyyy cut off xx/ return yyyy */
  
void encodeErgeMethods(struct track *tg);
/* setup special methods for ENCODE dbERGE II tracks */

Color encodeStanfordNRSFColor(struct track *tg, void *item, struct vGfx *vg);
/* different color for negative strand */

void encodeStanfordNRSFMethods(struct track *tg);
/* custom methods for ENCODE Stanford NRSF data */

#endif /* ENCODE_H */
