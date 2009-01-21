crTreeIndexBed -blockSize=8 test.bed test.cr
crTreeSearchBed test.bed test.cr chr1	144743000	144779000 > test.out
diff test.expected test.out
