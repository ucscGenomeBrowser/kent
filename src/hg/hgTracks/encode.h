#ifndef ENCODE_H
#define ENCODE_H

char *encodeErgeName(struct track *tg, void *item);
/* return the actual data name, in form xx/yyyy cut off xx/ return yyyy */
  
void encodeErgeMethods(struct track *tg);
/* setup special methods for ENCODE dbERGE II tracks */

#endif /* ENCODE_H */
