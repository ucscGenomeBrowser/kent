#ifndef BZ_CENSUS_H
#define BZ_CENSUS_H
typedef unsigned char census_t;
void msp_census(census_t census[], int n, msp_table_t *mt);
int census_mask_align(align_t *a, int n, uchar *fwd, uchar *rev, census_t *census, int thresh);
int census_mask_interval(int n, uchar *fwd, uchar *rev, int a, int z, census_t census[], int k);
void print_census(FILE *fp, census_t census[], int n, int k);
void print_intervals(FILE *fp, uchar census[], int n, int t);
census_t *new_census(int n);
#endif
