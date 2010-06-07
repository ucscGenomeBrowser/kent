/* jpegSize - read a jpeg header and figure out dimensions of image.
 * Adapted by Galt Barber from Matthias Wandel's jhead program */

#ifndef JPEGSIZE_H

void jpegSize(char *fileName, int *width, int *height);
/* Read image width and height.
 * Parse marker stream until SOS or EOI; */

#endif /* JPEGSIZE_H */

