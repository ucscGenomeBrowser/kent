table ggMrnaBlock
" A single block of an mRNA alignment."
(
	int qStart; "Start of block in query"
	int qEnd; "End of block in query"
	int tStart; "Start of block in target"
	int tEnd; "End of block in target"
)			

table ggMrnaAli
" An mRNA alignment, little richer data format than psl "
(
    string tName; "target that this alignment it to "
    int tStart; "start in target sequence "
    int tEnd; "end  in target sequence "
    char[3] strand; "+ or - depending on which strand alignment is to "
    string qName; "name (accession) of mRNA "
    int qStart; "start of alignment in query "
    int qEnd; "end of alignment in query "
    uint baseCount; "number of bases in query "
    short orientation; "1 or -1 orientation query appears to be in given biological evidence (i.e. splice site)  0 indicates no evidence. This could disagree with strand if est submitted rc'd for example " 
    int hasIntrons; "TRUE if intron present, FALSE otherwise"
    short milliScore; "Score 0-1000 "
    short blockCount; "Number of blocks. "
    table ggMrnaBlock[blockCount] blocks; "Dynamically allocated array. "
)
