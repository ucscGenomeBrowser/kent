blast_table_t *blast_1219_new(SEQ *seq, int W);
void blast_1219_free(blast_table_t *bt);
msp_table_t *blast_1219_search(SEQ *seq1, SEQ *seq2, blast_table_t *bt, msp_table_t *mt, ss_t ss, int X, int K, int P);
