Here's a table showing the Improberator score on random
data of various sizes.  The data was generated using a
1st order Markov model based on the yeast promoters,
which was also used as the null model.  Two sets of random
data were generated for each sequence size and number of 
sequences and the score of the two averaged. The 
Improberator was set to allow up to two matches to each
sequence.  The scores for the first motif found is in the
first table, the second motif in the second.

The score does seem to go up a little faster than log of
sequence size, and down a bit slower than log of number
of sequences.  

1st Motif
  Score 
                   Number of Sequences
        10   20   30   40   50   60   70   80   90   100
S  10  2.59 1.76 1.38 1.34 1.33 1.36 1.29 1.28 1.40 1.26
e  20  4.00 3.27 2.73 2.58 2.54 2.46 2.50 2.53 2.36 2.55
q  30  5.45 4.11 3.57 3.50 3.43 3.33 3.31 3.37 3.41 3.17
   40  6.45 4.96 4.25 4.23 4.08 3.99 4.03 3.91 3.96 3.91
S  50  6.83 5.28 5.04 4.95 4.68 4.57 4.59 4.51 4.36 4.39
i  60  8.25 6.71 5.64 5.26 5.19 4.97 4.84 4.97 4.77 4.77
z  70  8.03 6.52 5.93 5.49 5.44 5.44 5.25 5.21 5.23 5.13
e  80  9.14 7.13 6.30 5.89 5.70 5.69 5.63 5.57 5.54 5.41
   90  9.06 7.47 6.59 6.48 6.03 5.99 5.94 5.81 5.73 5.65


2nd Motif
  Score
                   Number of Sequences
        10   20   30   40   50   60   70   80   90   100
S  10  1.74 1.14 1.06 1.07 0.86 0.75 1.08 1.03 0.89 0.88
e  20  2.90 2.08 1.87 1.65 1.82 1.75 2.05 1.93 1.91 1.68
q  30  3.88 3.24 3.07 2.44 2.61 2.58 2.46 2.43 2.81 2.82
   40  4.82 3.81 3.81 3.39 3.23 3.37 3.28 3.50 3.12 3.02
S  50  5.10 4.67 4.19 3.90 3.85 4.04 3.80 3.60 3.78 3.84
i  60  6.21 4.67 4.31 4.60 4.35 4.38 4.36 3.98 4.02 4.16
z  70  6.87 5.48 5.06 4.94 4.62 4.58 4.81 4.59 4.59 4.48
e  80  7.03 6.15 5.34 5.04 5.00 5.00 5.08 5.10 4.94 4.83
   90  7.56 6.05 5.76 5.52 5.19 5.05 5.16 5.16 5.02 5.14
