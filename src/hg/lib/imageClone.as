table imageClone
"for use with image consortium's cumulative_plate files at: ftp://image.llnl.gov/image/outgoing"
(
uint id; "IMAGE cloneID"
string library; "Clone collection (LLAM for amp-resistant libraries, LLCM, for chloramphenicol-resistant libraries, LLKM for kanamycin-resistant libraries.  No rearray locations are given.)"
uint plateNum; "Plate number"
string row; "Row"
uint column; "Column"
uint libId; "IMAGE library ID"
string organism; "Species"
int numGenbank; "number of genbank records"
string[numGenbank] genbankIds; "Genbank accession number(s)"
)
