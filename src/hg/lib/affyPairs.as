table affyPairs
"Representation of the 'pairs' file format from the Affymetrix transcriptome data"
(
uint x; "X, the x-coordinate of the perfect-complement oligo on the chip."
uint y; "Y, the y-coordinate of the perfect-complement oligo on the chip."
string probeSet; "set, the probe set."
string method; "Method (not informative)"
uint tBase; "tBase, the target base (base in target at central position of 25-mer probe)."
uint pos; "Pos, the position in the probeset of the central base of the probe."
float pm; "PM, the perfect-complement probe intensity."
float pms; "PM.s, the standard deviation of the pixels in the perfect-complement feature."
float pmp; "PM.p, the number of pixel used in the perfect-complement feature."
float mm; "MM, the mismatch-complement probe intensity."
float mms; "MM.s, the standard deviation of the pixels in the mismatch-complement feature."
float mmp; "MM.p, the number of pixel used in the mismatch-complement feature. "
)
