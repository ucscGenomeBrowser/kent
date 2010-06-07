table affyTransLifted
"Data file format for Affymetrix transcriptome data from Simon Cawley which has been normalized and lifted to a new assembly of the human genome."
(
 string chrom;  "Reference sequence chromosome or scaffold"
 uint chromPos; "Coordinate in chromosome (location of central base of the 25-mer)"
 int xCoord;    "x-coordinate (column) of PM feature on chip"
 int yCoord;    "y-coordinate (row) of PM feature on chip"
 int rawPM;     "raw value of PM"
 int rawMM;     "raw value of MM"
 float normPM;  "normalized value of PM"
 float normMM;  "normalized value of MM"
)

