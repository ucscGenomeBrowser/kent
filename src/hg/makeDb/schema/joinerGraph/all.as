table affy10K
"Information from affy10K representing the Affymetrix 10K Mapping array"
    (
    string chrom; " "
    uint chromStart; " "
    uint chromEnd; " "
    uint affyId; "Affymetrix SNP id"
    )

table affy10KDetails
"Information from affy10KDetails representing the Affymetrix 10K Mapping Array"
(	string     affyId;    "Affymetrix SNP id"
	string     rsId;      "RS identifier (some are null)"
	string     tscId;     "TSC identifier (some are null)"
	char[2]    baseA;     "The first allele (A)"
	char[2]    baseB;     "The second allele (B)"
	char[34]   sequenceA; "The A allele with flanking sequence"
	char[34]   sequenceB; "The B allele with flanking sequence"
	char[8]    enzyme;    "The enzyme that was used to prepare the sample (HindIII or XbaI)"
)

table affy120KDetails
"Information from affyGenoDetails representing the Affymetrix 120K SNP Genotyping array"
(	string     affyId;    "Affymetrix SNP id"
	string     rsId;      "RS identifier (some are null)"
	char[2]    baseA;     "The first allele (A)"
	char[2]    baseB;     "The second allele (B)"
	char[34]   sequenceA; "The A allele with flanking sequence"
	char[34]   sequenceB; "The B allele with flanking sequence"
	char[8]    enzyme;    "The enzyme that was used to prepare the sample (HindIII or XbaI)"
	float      minFreq;   "The minimum allele frequency"
	float      hetzyg;    "The heterozygosity from all observations"
	float      avHetSE;   "The Standard Error for the average heterozygosity (not used)"
	char[2]    NA04477;   "Individual 01"
	char[2]    NA04479;   "Individual 02"
	char[2]    NA04846;   "Individual 03"
	char[2]    NA11036;   "Individual 04"
	char[2]    NA11038;   "Individual 05"
	char[2]    NA13056;   "Individual 06"
	char[2]    NA17011;   "Individual 07"
	char[2]    NA17012;   "Individual 08"
	char[2]    NA17013;   "Individual 09"
	char[2]    NA17014;   "Individual 10"
	char[2]    NA17015;   "Individual 11"
	char[2]    NA17016;   "Individual 12"
	char[2]    NA17101;   "Individual 13"
	char[2]    NA17102;   "Individual 14"
	char[2]    NA17103;   "Individual 15"
	char[2]    NA17104;   "Individual 16"
	char[2]    NA17105;   "Individual 17"
	char[2]    NA17106;   "Individual 18"
	char[2]    NA17201;   "Individual 19"
	char[2]    NA17202;   "Individual 20"
	char[2]    NA17203;   "Individual 21"
	char[2]    NA17204;   "Individual 22"
	char[2]    NA17205;   "Individual 23"
	char[2]    NA17206;   "Individual 24"
	char[2]    NA17207;   "Individual 25"
	char[2]    NA17208;   "Individual 26"
	char[2]    NA17210;   "Individual 27"
	char[2]    NA17211;   "Individual 28"
	char[2]    NA17212;   "Individual 29"
	char[2]    NA17213;   "Individual 30"
	char[2]    PD01;      "Individual 31"
	char[2]    PD02;      "Individual 32"
	char[2]    PD03;      "Individual 33"
	char[2]    PD04;      "Individual 34"
	char[2]    PD05;      "Individual 35"
	char[2]    PD06;      "Individual 36"
	char[2]    PD07;      "Individual 37"
	char[2]    PD08;      "Individual 38"
	char[2]    PD09;      "Individual 39"
	char[2]    PD10;      "Individual 40"
	char[2]    PD11;      "Individual 41"
	char[2]    PD12;      "Individual 42"
	char[2]    PD13;      "Individual 43"
	char[2]    PD14;      "Individual 44"
	char[2]    PD15;      "Individual 45"
	char[2]    PD16;      "Individual 46"
	char[2]    PD17;      "Individual 47"
	char[2]    PD18;      "Individual 48"
	char[2]    PD19;      "Individual 49"
	char[2]    PD20;      "Individual 50"
	char[2]    PD21;      "Individual 51"
	char[2]    PD22;      "Individual 52"
	char[2]    PD23;      "Individual 53"
	char[2]    PD24;      "Individual 54"
)

table affyAtlas
"analysis information from Affymetrix human atlas data"
(
string annName; "analysis name, array name?"
string probeSet; "probe set that signal corresponds to"
float signal; "signal of probeset detected"
char[2] detection; "not sure..."
float pval; "p-value"
string tissue; "tissue sample comes from"
)

table affyGenoDetails
"Information from affyGenoDetails representing the Affymetrix 120K SNP array"
(	uint       affyId;    "Affymetrix SNP id"
	uint       rsId;      "RS identifier (some are null)"
	char[2]    baseA;     "The first allele (A)"
	char[2]    baseB;     "The second allele (B)"
	char[34]   sequenceA; "The A allele with flanking sequence"
	char[34]   sequenceB; "The B allele with flanking sequence"
	char[8]    enzyme;    "The enzyme that was used to prepare the sample (HindIII or XbaI)"
	float      minFreq;   "The minimum allele frequency"
	float      hetzyg;    "The heterozygosity from all observations"
	float      avHetSE;   "The Standard Error for the average heterozygosity (not used)"
	char[2]    NA04477;   "Individual 01"
	char[2]    NA04479;   "Individual 02"
	char[2]    NA04846;   "Individual 03"
	char[2]    NA11036;   "Individual 04"
	char[2]    NA11038;   "Individual 05"
	char[2]    NA13056;   "Individual 06"
	char[2]    NA17011;   "Individual 07"
	char[2]    NA17012;   "Individual 08"
	char[2]    NA17013;   "Individual 09"
	char[2]    NA17014;   "Individual 10"
	char[2]    NA17015;   "Individual 11"
	char[2]    NA17016;   "Individual 12"
	char[2]    NA17101;   "Individual 13"
	char[2]    NA17102;   "Individual 14"
	char[2]    NA17103;   "Individual 15"
	char[2]    NA17104;   "Individual 16"
	char[2]    NA17105;   "Individual 17"
	char[2]    NA17106;   "Individual 18"
	char[2]    NA17201;   "Individual 19"
	char[2]    NA17202;   "Individual 20"
	char[2]    NA17203;   "Individual 21"
	char[2]    NA17204;   "Individual 22"
	char[2]    NA17205;   "Individual 23"
	char[2]    NA17206;   "Individual 24"
	char[2]    NA17207;   "Individual 25"
	char[2]    NA17208;   "Individual 26"
	char[2]    NA17210;   "Individual 27"
	char[2]    NA17211;   "Individual 28"
	char[2]    NA17212;   "Individual 29"
	char[2]    NA17213;   "Individual 30"
	char[2]    PD01;      "Individual 31"
	char[2]    PD02;      "Individual 32"
	char[2]    PD03;      "Individual 33"
	char[2]    PD04;      "Individual 34"
	char[2]    PD05;      "Individual 35"
	char[2]    PD06;      "Individual 36"
	char[2]    PD07;      "Individual 37"
	char[2]    PD08;      "Individual 38"
	char[2]    PD09;      "Individual 39"
	char[2]    PD10;      "Individual 40"
	char[2]    PD11;      "Individual 41"
	char[2]    PD12;      "Individual 42"
	char[2]    PD13;      "Individual 43"
	char[2]    PD14;      "Individual 44"
	char[2]    PD15;      "Individual 45"
	char[2]    PD16;      "Individual 46"
	char[2]    PD17;      "Individual 47"
	char[2]    PD18;      "Individual 48"
	char[2]    PD19;      "Individual 49"
	char[2]    PD20;      "Individual 50"
	char[2]    PD21;      "Individual 51"
	char[2]    PD22;      "Individual 52"
	char[2]    PD23;      "Individual 53"
	char[2]    PD24;      "Individual 54"
)
table affyOffset
"File format giving offset of Affymetrix probe sets into contigs."
(
string piece; "Name of 'piece' of genome, something like ctg21fin1piece100"
uint tStart;  "Start of 'piece' in contig."
)
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
table affyTransLifted
"Data file format for Affymetrix transcriptome data from Simon Cawley which has been normalized and lifted to a new assembly of the human genome."
(
 string chrom;  "Chromosome in hs.NCBIfreeze.chrom fomat originally"
 uint chromPos; "Coordinate in chromosome (location of central base of the 25-mer)"
 int xCoord;    "x-coordinate (column) of PM feature on chip"
 int yCoord;    "y-coordinate (row) of PM feature on chip"
 int rawPM;     "raw value of PM"
 int rawMM;     "raw value of MM"
 float normPM;  "normalized value of PM"
 float normMM;  "normalized value of MM"
)

table affyTranscriptome
"Affymetrix Transcriptome"
(
	string	chrom;		"Chromosome"
	uint 	chromStart;	"Start in Chromosome"
	uint	chromEnd;	"End in Chromosome"
	string	name;		"Name"
	uint	score;		"Score"
	char[1]	strand;		"Strand"
	uint	sampleCount;	"Number of points in this record"
	lstring	samplePosition;	"Posititions of the oligos"
	lstring	sampleHeight;	"Intentisy of hybridization"
)
table agpFrag
"How to get through chromosome based on fragments"
    (
    string chrom;	"which chromosome"
    uint chromStart;	"start position in chromosome"
    uint chromEnd;	"end position in chromosome"
    int ix;             "ix of this fragment (useless)"
    char[1] type;       "(P)redraft, (D)raft, (F)inished or (O)ther"
    string frag;        "which fragment"
    uint fragStart;     "start position in frag"
    uint fragEnd;       "end position in frag"
    char[1] strand;     "+ or - (orientation of fragment)"
    )

table agpGap
"Gaps in golden path"
    (
    string chrom;	"which chromosome"
    uint chromStart;	"start position in chromosome"
    uint chromEnd;	"end position in chromosome"
    int ix;             "ix of this fragment (useless)"
    char[1] n;          "always 'N'"
    uint size;          "size of gap"
    string type;        "contig, clone, fragment, etc."
    string bridge;      "yes, no, mrna, bacEndPair, etc."
    )

table altGraph 
"An alternatively spliced gene graph."
    (
    uint id;                        "Unique ID"
    string tName;                   "name of target sequence, often a chrom."
    int tStart;                    "First bac touched by graph"
    int tEnd;                      "Start position in first bac"
    char[2] strand;                 "+ or - strand"
    uint vertexCount;               "Number of vertices in graph"
    ubyte[vertexCount] vTypes;      "Type for each vertex"
    int[vertexCount] vPositions;   "Position in target for each vertex"
    uint edgeCount;                 "Number of edges in graph"
    int[edgeCount] edgeStarts;     "Array with start vertex of edges"
    int[edgeCount] edgeEnds;       "Array with end vertex of edges."
    int mrnaRefCount;               "Number of supporting mRNAs."
    string[mrnaRefCount] mrnaRefs;  "Ids of mrnas supporting this." 
   )

object evidence
"List of mRNA/ests supporting a given edge"
(
    int evCount;                   "number of ests evidence"
    int [evCount] mrnaIds;         "ids of mrna evidence, indexes into altGraphx->mrnaRefs"
)

table altGraphX
"An alternatively spliced gene graph."
    (
    string tName;                   "name of target sequence, often a chrom."
    int tStart;                     "First bac touched by graph."
    int tEnd;                       "Start position in first bac."
    string name;                    "Human readable name."
    uint id;                        "Unique ID."	
    char[2] strand;                 "+ or - strand."
    uint vertexCount;               "Number of vertices in graph."
    ubyte[vertexCount] vTypes;      "Type for each vertex."
    int[vertexCount] vPositions;    "Position in target for each vertex."
    uint edgeCount;                 "Number of edges in graph."
    int[edgeCount] edgeStarts;      "Array with start vertex of edges."
    int[edgeCount] edgeEnds;        "Array with end vertex of edges."
    table evidence[edgeCount] evidence;  "array of evidence tables containing references to mRNAs that support a particular edge."
    int[edgeCount] edgeTypes;       "Type for each edge, ggExon, ggIntron, etc."
    int mrnaRefCount;               "Number of supporting mRNAs."
    string[mrnaRefCount] mrnaRefs;  "Ids of mrnas supporting this." 
    int[mrnaRefCount] mrnaTissues;  "Ids of tissues that mrnas come from, indexes into tissue table"
    int[mrnaRefCount] mrnaLibs;     "Ids of libraries that mrnas come from, indexes into library table"
   )

table atlasOncoGene
"Table used to link into ATLAS Oncology site"
   (
   string locusSymbol;	"LocusLink Symbol"
   string atlasGene;	"ATLAS Gene" 
   string otherGene;	"Other gene"
   string url;		"URL for corresonding ATLAS web page"
   )

table axtInfo
"Axt alignment names and sizes"
    (
    char[25] species; "long name of species"
    char[35] alignment; "name of alignment"
    string chrom;	"chromosome name"
    string fileName;    "axt  file name "
    int sort;          "sort order"
    )
table bactigPos
"Bactig positions in chromosome coordinates (bed 4 +)."
    (
    string chrom;        "Chromosome"
    uint   chromStart;   "Start position in chromosome"
    uint   chromEnd;     "End position in chromosome"
    string name;         "Bactig"
    string startContig;  "First contig in this bactig"
    string endContig;    "Last contig in this bactig"
    )
table bdgpExprLink
"Summary info for linking to the BDGP Expression In Situ Images site"
    (
    string symbol;       "Symbolic gene name"
    string bdgpName;     "BDGP gene ID"
    string flyBaseId;    "FlyBase gene ID"
    string est;          "EST accession"
    int imageCount;      "Number of images for this gene"
    int bodyPartCount;   "Number of body parts"
    string plate;        "DGC Plate"
    int platePos;        "Position on plate"
    char newRelease;     "'Y' if this gene is a new release"
    )
table bdgpGeneInfo
"Berkeley Drosophile Genome Project Protein-Coding Genes track additional information"
    (
    string bdgpName;		"BDGP annotation gene name"
    string flyBaseId;		"FlyBase FBgn ID"
    lstring go;			"comma-sep list of GO numeric IDs"
    string symbol;		"symbolic gene name"
    string cytorange;		"cytorange"
    string cdna_clone;		"comma-sep list of cdna clone IDs"
    )
table bdgpSwissProt
"Berkeley Drosophile Genome Project Protein-Coding Genes track additional information plus SwissProt acc & desc"
    (
    string bdgpName;		"BDGP annotation gene name"
    string flyBaseId;		"FlyBase FBgn ID"
    lstring go;			"comma-sep list of GO numeric IDs"
    string symbol;		"symbolic gene name"
    string cytorange;		"cytorange"
    string swissProtId;		"SwissProt ID"
    string spGeneName;		"(long) gene name from SwissProt"
    string spSymbol;		"symbolic-looking gene id from SwissProt"
    )
table bed4
"Browser extensible data"
    (
    string chrom;      "Human chromosome or FPC contig"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    )
table bed6
"Browser extensible data"
    (
    string chrom;      "Human chromosome or FPC contig"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    )
table bgiGeneInfo
"Beijing Genomics Institute Genes track additional information"
    (
    string name;	"BGI annotation gene name"
    string source;	"Source of gene annotation"
    lstring go;		"comma-sep list of GO numeric IDs"
    lstring ipr;	"semicolon-sep list of IPR numeric IDs and comments"
    )
table bgiGeneSnp
"Beijing Genomics Institute Gene-SNP associations (many-to-many)"
    (
    string geneName;	"Name of BGI gene."
    string snpName;	"Name of BGI SNP"
    string geneAssoc;	"Association to gene: upstream, utr, exon, etc"
    string effect;	"Changes to codon or splice site (if applicable)"
    char[1] phase;	"Phase of SNP in codon (if applicable)"
    string siftComment;	"Comment from SIFT (if applicable)"
    )
table bgiSnp
"Beijing Genomics Institute SNP information as bed 4 +"
    (
    string chrom;	"Chromosome"
    uint   chromStart;	"Start position in chromosome"
    uint   chromEnd;	"End position in chromosome"
    string name;	"BGI SNP name: snp.superctg.ctg.pos.type.strainID"
    char[1] snpType;	"S (substitution), I (insertion), or D (deletion)"
    uint readStart;	"Start position in alternate allele read"
    uint readEnd;	"End position in alternate allele read"
    uint qualChr;	"Quality score in reference assembly"
    uint qualReads;	"Quality score in alternate allele read"
    string snpSeq;	"'X->Y' or indel sequence"
    string readName;	"Name of alternate allele read"
    char[1] readDir;    "Direction of read relative to reference"
    char[4] inBroiler;	"SNP found in Broiler strain? yes, no or n/a if not covered"
    char[4] inLayer;	"SNP found in Layer strain? yes, no or n/a if not covered"
    char[4] inSilkie;	"SNP found in Silkie strain? yes, no or n/a if not covered"
    string primerL;	"Left primer sequence"
    string primerR;	"Right primer sequence"
    char[1] questionM;	"L for dubious indels, H for other indels and SNPs"
    string extra;	"Additional information"
    )
table bioCycMapDesc 
"Description of BioCyc pathway maps"
   (
   string mapID; 	"BioCyc pathway map"
   string description; 	"BioCyc pathway map description"
   )
table bioCycPathway 
"BioCyc Pathway to Known Gene cross reference"
   (
   string kgID; 	"Known Gene ID"
   string geneID; 	"Gene (RefSeq) ID"
   string mapID; 	"BioCyc pathway map ID"
   )

table blastTab
"Tab-delimited blast output file"
    (
    string query;	"Name of query sequence"
    string target;	"Name of target sequence"
    float identity;	"Percent identity"
    uint aliLength;	"Length of alignment"
    uint mismatch;	"Number of mismatches"
    uint gapOpen;	"Number of gap openings"
    uint qStart;	"Start in query (0 based)"
    uint qEnd;		"End in query (non-inclusive)"
    uint tStart;	"Start in target (0 based)"
    uint tEnd;		"End in target (non-inclusive)"
    double eValue;	"Expectation value"
    double bitScore;	"Bit score"
    )

table blastzNet
"blastz netting file "
(
string chrom;       "Human Chrom"
uint chromStart;       "Start on Human"
uint chromEnd;         "End on Human"
string name;   "Mouse Chromosome"
uint score;     "score always zero"
char[1] strand; "+ direction matches - opposite"
uint thickStart;       "Start on Human"
uint thickEnd;         "End on Human"
)

table blatServers
"Description of online BLAT servers"
    (
    string db;	"Database name"
    string host;	"Host (machine) name"
    int port;	"TCP/IP port on host"
    byte isTrans;  "0 for nucleotide/1 for translated nucleotide"
    )
table borf
"Parsed output from Victor Solovyev's bestorf program"
    (
    string name; "Name of mRNA"
    int size;   "Size of mRNA"
    char[1] strand; "+ or - or . in empty case"
    string feature; "Feature name - Always seems to be CDSo"
    int cdsStart;  "Start of cds (starting with 0)"
    int cdsEnd;    "End of cds (non-exclusive)"
    float score;   "A score of 50 or more is decent"
    int orfStart;  "Start of orf (not always same as CDS)"
    int orfEnd;  "Start of orf (not always same as CDS)"
    int cdsSize; "Size of cds in bases"
    char[3] frame; "Seems to be +1, +2, +3 or -1, -2, -3"
    lstring protein; "Predicted protein.  May be empty."
    )
table borkPseudoHom
"stats and protein homologies behind Bork pseudogenes"
    (
    string name;	"Bork pseudogene gene name"
    string protRef;	"String with swissprot or trembl reference "
    lstring description; "Freeform (except for no tabs) description"
    )
table cartDb
"A simple id/contents pair for persistent storing of cart variables"
    (
    uint id;	"Cart ID"
    lstring contents; "Contents - encoded variables"
    int reserved;     "Reserved field, currently always zero"
    string firstUse; "First time this was used"
    string lastUse; "Last time this was used"
    int useCount; "Number of times used"
    )
table celeraCoverage
"Summary of large genomic Duplications from Celera Data"
        (
        string chrom;	"Human chromosome or FPC contig"
        uint chromStart;	"Start position in chromosome"
	uint chromEnd;	"End position in chromosome"
	string name; "Source of Information"
	)
table celeraDupPositive
"Summary of large genomic Duplications from Celera Data"
        (
        string chrom;	"Human chromosome or FPC contig"
        uint chromStart;	"Start position in chromosome"
	uint chromEnd;	"End position in chromosome"
	string name;	"Celera accession Name"
	string fullname;	"Celera Accession Fullname"
	float fracMatch;	"fraction of matching bases"
	float bpalign;	"base pair alignment score"
	)

table cgapAlias
"CGAP alias"
   (
   string cgapID; 	"CGAP patheay ID"
   string alias; 	"Gene symbol or mRNA"
   )
table cgapBiocDesc
"CGAP/BioCarta pathway description"
  (
  string mapID;		"CGAP/BioCarta pathway map ID"
  string description;	"description"
  )

table cgapBiocPathway
"CGAP BioCarta pathway cross reference"
   (
   string cgapID; 	"CGAP pathway ID"
   string mapID; 	"BioCarta pathway ID"
   )
table cgh
"Comparative Genomic Hybridization data assembly position information"
    (
    string chrom; "Chromosome name"
    uint chromStart; "position in nucleotides where feature starts on chromosome"
    uint chromEnd; "position in nucleotides where featrure stops on chromsome"
    string name;  "Name of the cell line (type 3 only)"
    float score; "hybridization score"
    uint type;  "1 - overall average, 2 - tissue average, 3 - single tissue"
    string tissue;  "Type of tissue cell line derived from (type 2 and type 3)"
    string clone; "Name of clone"
    uint spot;  "Spot number on array"
    )
table chain
"Summary info about a chain of alignments"
    (
    double score; "score of chain"
    string tName; "Target sequence name"
    uint tSize; "Target sequence size"
    uint tStart; "Alignment start position in target"
    uint tEnd; "Alignment end position in target"
    string qName; "Query sequence name"
    uint qSize; "Query sequence size"
    char qStrand; "Query strand"
    uint qStart; "Alignment start position in query"
    uint qEnd; "Alignment end position in query"
    uint id; "chain id"
    )
table chainGap
"alignment block in chain"
    (
    string tName; "Target sequence name"
    uint tStart; "Alignment start position in target"
    uint tEnd; "Alignment end position in target"
    uint qStart; "start in query"
    uint chainId; "chain id in chain table"
    )
table chainLink
"alignment block in chain"
    (
    string tName; "Target sequence name"
    uint tStart; "Alignment start position in target"
    uint tEnd; "Alignment end position in target"
    uint qStart; "start in query"
    uint chainId; "chain id in chain table"
    )
table chr18deletions
"Chromosome 18 deletion"
    (
    string chrom;	"Chromosome or 'unknown'"
    int chromStart;     "Start position in chrom - negative 1 if unpositioned"
    uint chromEnd;	"End position in chrom"
    string name;	"Name of STS marker"
    uint score;	        "Score of a marker = 1000/(# of placements)"
    char[1] strand;	"Strand = or -"
    uint ssCount;	"Number of small blocks"
    uint[ssCount] smallStarts;	"Small start positions"
    uint seCount;	"Number of small blocks"
    uint[seCount] smallEnds;	"Small end positions"
    uint lsCount;	"Number of large blocks"
    uint[lsCount] largeStarts;	"Large start positions"
    uint leCount;	"Number of large blocks"
    uint[leCount] largeEnds;    	"Large end positions"
    )
table chromInfo
"Chromosome names and sizes"
    (
    string chrom;	"Chromosome name"
    uint size;          "Chromosome size"
    string fileName;    "Chromosome file (raw one byte per base)"
    )
table clonePos
"A clone's position and other info."
    (
    string name;	"Name of clone including version"
    uint seqSize;       "base count not including gaps"
    ubyte phase;        "htg phase"
    string chrom;       "Chromosome name"
    uint chromStart;	"Start in chromosome"
    uint chromEnd;      "End in chromosome"
    char[1] stage;      "F/D/P for finished/draft/predraft"
    string faFile;      "File with sequence."
    )
table columnInfo
"Meta data information about a particular column in the database."
(
string name; 	"Column name"
string type;    "Column type, i.e. int, blob, varchar."
string nullAllowed; "Are NULL values allowed?"
string key;     "Is this field indexed? If so how."
string defaultVal; "Default value filled."
string extra;   "Any extra information."
)
table cpgIsland
"Describes the CpG Islands"
   (
   string chrom;      	"Human chromosome or FPC contig"
   uint   chromStart; 	"Start position in chromosome"
   uint   chromEnd;   	"End position in chromosome"
   string name;       	"CpG Island"
   uint   length;    	"Island Length"
   uint   cpgNum;    	"Number of CpGs in island"
   uint   gcNum;    	"Number of C and G in island"
   float   perCpg;  	"Percentage of island that is CpG"
   float   perGc;  	"Percentage of island that is C or G"
   ) 
table ctgPos
"Where a contig is inside of a chromosome."
    (
    string contig;	"Name of contig"
    uint size;		"Size of contig"
    string chrom;       "Chromosome name"
    uint chromStart;	"Start in chromosome"
    uint chromEnd;       "End in chromosome"
    )
table cytoBand
"Describes the positions of cytogenetic bands with a chromosome"
   (
   string chrom;    "Human chromosome number"
   uint   chromStart;  "Start position in genoSeq"
   uint   chromEnd;    "End position in genoSeq"
   string name;       "Name of cytogenetic band"

   string   gieStain; "Giesma stain results"
   uint   firmStart;   "Start position of part we're confident is in band"
   uint   firmEnd;     "End position of part we're confident is in band"
   )
table dbDb
"Description of annotation database"
    (
    string  name;	"Short name of database.  'hg8' or the like"
    string description; "Short description - 'Aug. 8, 2001' or the like"
    lstring nibPath;	"Path to packed sequence files"
    string organism;    "Common name of organism - first letter capitalized"
    string defaultPos;	"Default starting position"
    int active;     "Flag indicating whether this db is in active use"
    int orderKey;     "Int used to control display order within a genome"
    string genome;    "Unifying genome collection to which an assembly belongs"
    string scientificName;  "Genus and species of the organism; e.g. Homo sapiens"
    string htmlPath;  "path in /gbdb for assembly description"
    ubyte hgNearOk; "Have hgNear for this?"
    )
table dbSnpRs "Information from dbSNP at the reference SNP level"
    (
    string  rsId;    	"dbSnp reference snp (rs) identifier"
    float   avHet;   	"the average heterozygosity from all observations"
    float   avHetSE; 	"the Standard Error for the average heterozygosity"
    string  valid;   	"the validation status of the SNP"
    string  allele1;   	"the sequence of the first allele"
    string  allele2;   	"the sequence of the second allele"
    string  assembly; 	"the sequence in the assembly"
    string  alternate; 	"the sequence of the alternate allele"
    string  func; 	"the functional category of the SNP, if any"
    )
table defaultDb
"Description of annotation database"
    (
    string genome;    "Unifying genome collection for which an assembly is the default"
    string  name;	"Short name of database.  'hg8' or the like"
    )
table distance
"Abstract distance between two genes"
    (
    char query;  "Query gene.  Indexed"
    char target; "Target gene. Not indexed"
    float distance; "Distance between two, possibly in some expression space"
    )
table dnaMotif
"A gapless DNA motif"
    (
    string name;			"Motif name."
    int columnCount;			"Count of columns in motif."
    float[columnCount] aProb;		"Probability of A's in each column."
    float[columnCount] cProb;		"Probability of C's in each column."
    float[columnCount] gProb;		"Probability of G's in each column."
    float[columnCount] tProb;		"Probability of T's in each column."
    )


table dupSpMrna
"Duplicated mRNA/protein IDs corresponding to a Known Genen mRNA"
   (
   string mrnaID; 	"Known Gene (mRNA) ID"
   string proteinID; 	"protein ID for this Known Gene"
   string dupMrnaID; 	"mrnaID for the duplicate"
   string dupProteinID; "protein ID for this duplicate"
   )
table encodeErge
"ENCODE experimental data from dbERGEII"
   (
   string           chrom;       "Human chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Name of read - up to 255 characters"
   uint             score;       "Score from 0-1000.  1000 is best"
   char[1]          strand;      "Value should be + or -"
   uint             thickStart;  "Start of where display should be thick (start codon)"
   uint             thickEnd;    "End of where display should be thick (stop codon)"
   uint             reserved;    "Always zero for now"
   uint             blockCount;  "Number of separate blocks (regions without gaps)"
   uint[blockCount] blockSizes;  "Comma separated list of block sizes"
   uint[blockCount] chromStarts; "Start position of each block in relative to chromStart"
   string           Id;          "dbERGEII Id"
   string           color;       "RGB color values"
   )
table encodeErge5race
"ENCODE experimental data from dbERGEII"
   (
   string           chrom;       "Human chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Name of read - up to 255 characters"
   uint             score;       "Score from 0-1000.  1000 is best"
   char[1]          strand;      "Value should be + or -"
   uint             thickStart;  "Start of where display should be thick (start codon)"
   uint             thickEnd;    "End of where display should be thick (stop codon)"
   uint             reserved;    "Always zero for now"
   uint             blockCount;  "Number of separate blocks (regions without gaps)"
   uint[blockCount] blockSizes;  "Comma separated list of block sizes"
   uint[blockCount] chromStarts; "Start position of each block in relative to chromStart"
   string           Id;          "dbERGEII Id"
   string           color;       "RGB color values"
   )
table encodeErgeBinding
"ENCODE experimental data from dbERGEII"
   (
   string           chrom;       "Human chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Name of read - up to 255 characters"
   uint             score;       "Score from 0-1000.  1000 is best"
   char[1]          strand;      "Value should be + or -"
   uint             thickStart;  "Start of where display should be thick (start codon)"
   uint             thickEnd;    "End of where display should be thick (stop codon)"
   uint             reserved;    "Always zero for now"
   uint             blockCount;  "Number of separate blocks (regions without gaps)"
   uint[blockCount] blockSizes;  "Comma separated list of block sizes"
   uint[blockCount] chromStarts; "Start position of each block in relative to chromStart"
   string           Id;          "dbERGEII Id"
   string           color;       "RGB color values"
   )
table encodeErgeDNAseI
"ENCODE experimental data from dbERGEII"
   (
   string           chrom;       "Human chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Name of read - up to 255 characters"
   uint             score;       "Score from 0-1000.  1000 is best"
   char[1]          strand;      "Value should be + or -"
   uint             thickStart;  "Start of where display should be thick (start codon)"
   uint             thickEnd;    "End of where display should be thick (stop codon)"
   uint             reserved;    "Always zero for now"
   uint             blockCount;  "Number of separate blocks (regions without gaps)"
   uint[blockCount] blockSizes;  "Comma separated list of block sizes"
   uint[blockCount] chromStarts; "Start position of each block in relative to chromStart"
   string           Id;          "dbERGEII Id"
   string           color;       "RGB color values"
   )
table encodeErgeExpProm
"ENCODE experimental data from dbERGEII"
   (
   string           chrom;       "Human chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Name of read - up to 255 characters"
   uint             score;       "Score from 0-1000.  1000 is best"
   char[1]          strand;      "Value should be + or -"
   uint             thickStart;  "Start of where display should be thick (start codon)"
   uint             thickEnd;    "End of where display should be thick (stop codon)"
   uint             reserved;    "Always zero for now"
   uint             blockCount;  "Number of separate blocks (regions without gaps)"
   uint[blockCount] blockSizes;  "Comma separated list of block sizes"
   uint[blockCount] chromStarts; "Start position of each block in relative to chromStart"
   string           Id;          "dbERGEII Id"
   string           color;       "RGB color values"
   )
table encodeErgeHssCellLines
"ENCODE experimental data from dbERGEII"
   (
   string           chrom;       "Human chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Name of read - up to 255 characters"
   uint             score;       "Score from 0-1000.  1000 is best"
   char[1]          strand;      "Value should be + or -"
   uint             thickStart;  "Start of where display should be thick (start codon)"
   uint             thickEnd;    "End of where display should be thick (stop codon)"
   uint             reserved;    "Always zero for now"
   uint             blockCount;  "Number of separate blocks (regions without gaps)"
   uint[blockCount] blockSizes;  "Comma separated list of block sizes"
   uint[blockCount] chromStarts; "Start position of each block in relative to chromStart"
   string           Id;          "dbERGEII Id"
   string           color;       "RGB color values"
   string           allLines;    "List of all cell lines"
   )
table encodeErgeInVitroFoot
"ENCODE experimental data from dbERGEII"
   (
   string           chrom;       "Human chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Name of read - up to 255 characters"
   uint             score;       "Score from 0-1000.  1000 is best"
   char[1]          strand;      "Value should be + or -"
   uint             thickStart;  "Start of where display should be thick (start codon)"
   uint             thickEnd;    "End of where display should be thick (stop codon)"
   uint             reserved;    "Always zero for now"
   uint             blockCount;  "Number of separate blocks (regions without gaps)"
   uint[blockCount] blockSizes;  "Comma separated list of block sizes"
   uint[blockCount] chromStarts; "Start position of each block in relative to chromStart"
   string           Id;          "dbERGEII Id"
   string           color;       "RGB color values"
   )
table encodeErgeMethProm
"ENCODE experimental data from dbERGEII"
   (
   string           chrom;       "Human chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Name of read - up to 255 characters"
   uint             score;       "Score from 0-1000.  1000 is best"
   char[1]          strand;      "Value should be + or -"
   uint             thickStart;  "Start of where display should be thick (start codon)"
   uint             thickEnd;    "End of where display should be thick (stop codon)"
   uint             reserved;    "Always zero for now"
   uint             blockCount;  "Number of separate blocks (regions without gaps)"
   uint[blockCount] blockSizes;  "Comma separated list of block sizes"
   uint[blockCount] chromStarts; "Start position of each block in relative to chromStart"
   string           Id;          "dbERGEII Id"
   string           color;       "RGB color values"
   )
table encodeErgeStableTransf
"ENCODE experimental data from dbERGEII"
   (
   string           chrom;       "Human chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Name of read - up to 255 characters"
   uint             score;       "Score from 0-1000.  1000 is best"
   char[1]          strand;      "Value should be + or -"
   uint             thickStart;  "Start of where display should be thick (start codon)"
   uint             thickEnd;    "End of where display should be thick (stop codon)"
   uint             reserved;    "Always zero for now"
   uint             blockCount;  "Number of separate blocks (regions without gaps)"
   uint[blockCount] blockSizes;  "Comma separated list of block sizes"
   uint[blockCount] chromStarts; "Start position of each block in relative to chromStart"
   string           Id;          "dbERGEII Id"
   string           color;       "RGB color values"
   )
table encodeErgeSummary
"ENCODE experimental data from dbERGEII"
   (
   string           chrom;       "Human chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Name of read - up to 255 characters"
   uint             score;       "Score from 0-1000.  1000 is best"
   char[1]          strand;      "Value should be + or -"
   uint             thickStart;  "Start of where display should be thick (start codon)"
   uint             thickEnd;    "End of where display should be thick (stop codon)"
   uint             reserved;    "Always zero for now"
   uint             blockCount;  "Number of separate blocks (regions without gaps)"
   uint[blockCount] blockSizes;  "Comma separated list of block sizes"
   uint[blockCount] chromStarts; "Start position of each block in relative to chromStart"
   string           Id;          "dbERGEII Id"
   string           color;       "RGB color values"
   )
table encodeErgeTransTransf
"ENCODE experimental data from dbERGEII"
   (
   string           chrom;       "Human chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Name of read - up to 255 characters"
   uint             score;       "Score from 0-1000.  1000 is best"
   char[1]          strand;      "Value should be + or -"
   uint             thickStart;  "Start of where display should be thick (start codon)"
   uint             thickEnd;    "End of where display should be thick (stop codon)"
   uint             reserved;    "Always zero for now"
   uint             blockCount;  "Number of separate blocks (regions without gaps)"
   uint[blockCount] blockSizes;  "Comma separated list of block sizes"
   uint[blockCount] chromStarts; "Start position of each block in relative to chromStart"
   string           Id;          "dbERGEII Id"
   string           color;       "RGB color values"
   )
table encodeRegionInfo
"Descriptive, assembly-independent information about ENCODE regions"
    (
    string name;	"Name of region"
    string descr;	"Description (gene region, random pick, etc.)"
    )
table ensGeneXref
"This is the table gene_xref downloaded directly from Ensembl ftp site (18.34 release 11/04). Please refer to Ensembl site for details.  CAUTION: Ensembl sometimes changes its table definitions and some field may not contais data as its name indicates, e.g. translation_name."
   (
   string db;			"unknown - check with Ensembl"
   string analysis;		"unknown - check with Ensembl"
   string type;			"unknown - check with Ensembl"
   int gene_id;			"gene ID"
   string gene_name;		"gene name"
   int[5] gene_version;		"gene version"
   int transcript_id;		"transcript  ID"
   string transcript_name;	"transcript  name"
   int[5] transcript_version;	"transcript  version"
   int[5] translation_name;	"translation name"
   int translation_id;		"translation ID"
   int[5] translation_version;	"translation version"
   string external_db;		"external database"
   string external_name;	"external name"
   char[10] external_status;	"external status"
   )
table ensPhusionBlast
"Ensembl PhusionBlast Synteny (bed 6 +)."
    (
    string chrom;        "$Organism chromosome"
    uint   chromStart;   "Start position in $Organism chromosome"
    uint   chromEnd;     "End position in $Organism chromosome"
    string name;         "Name of $Xeno chromosome"
    uint   score;        "Integer alignment score"
    char[1] strand;      "Direction of alignment: + or -"
    uint   xenoStart ;   "Start position in $Xeno chromosome"
    uint   xenoEnd;      "End position in $Xeno chromosome"
    )
table ensTranscript
"This is the table transcript downloaded directly from Ensembl ftp site (18.34 release 11/04). Please refer to Ensembl site for details.  CAUTION: Ensembl sometimes changes its table definitions and some field may not contais data as its name indicates."
   (
   int id;			"internal system ID?"
   string db;			"unknown - check with Ensembl"
   string analysis;		"unknown - check with Ensembl"
   string type;			"unknown - check with Ensembl"
   int transcript_id;		"transcript ID"
   string transcript_name;	"transcript name"
   int transcript_version;	"transcript version"
   string chr_name;		"chromosome number"
   int chr_start;		"start position"
   int chr_end;			"end   position"
   int chr_strand;		"strand"
   int coding_start;		"coding start position"
   int coding_end;		"coding end   position"
   int translation_id;		"translation ID"
   string translation_name;	"translation Name"
   string translation_version;	"translation version"
   int gene_id;			"gene ID"
   string gene_name;		"gene name"
   int gene_version;		"gene version"
   lstring exon_structure;	"exon structure"
   lstring exon_ids;		"exon IDs"
   string external_db;		"external database"
   string external_name;	"external name"
   char[10] external_status;	"external status"
   )

table erRegGeneToModule
"Module (group of transcription factor binding sites) for gene"
    (
    string gene;	"Name of gene"
    int module;		"ID of regulatory module, see also esRegModuleToMotif"
    )
table erRegGeneToMotif
"Predicted transcription factor binding site associated with gene"
    (
    string gene;	"Name of gene"
    string motif;	"Name of transcription factor if known, or motif ID"
    )
table esRegModuleToMotif
"Associates transcription factor binding sites that work together into modules"
    (
    int module;		"Module ID"
    string motif;	"Name of transcription factor if known, or motif ID"
    )
table est3
"EST 3-prime ends"
    (
    string chrom;   "Chromosome"
    uint chromStart; "Start position in chromosome"
    uint chromEnd;  "End position in chromosome"
    char[1] strand;  "+ or - strand"
    uint estCount;  "Number of ESTs supporting this"
    )

table estOrientInfo
"Extra information on ESTs - calculated by polyInfo program"
    (
    string chrom;      "Human chromosome or FPC contig"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Accession of EST"
    short intronOrientation; "Orientation of introns with respect to EST"
    short sizePolyA;   "Number of trailing A's"
    short revSizePolyA; "Number of trailing A's on reverse strand"
    short signalPos;   "Position of start of polyA signal relative to end of EST or 0 if no signal"
    short revSignalPos;"PolyA signal position on reverse strand if any"
    )
table estPair
"EST 5'- 3' pairing information"
	(
	string chrom;		"Chromosome name"
	uint chromStart;	"Start position in chromosome"
	uint chromEnd;		"End position in chromosome"
	string mrnaClone;	"Mrna clone name"
	string acc5;		"5' EST accession number"
	uint start5;		"Start position of 5' EST in chromosome"
	uint end5;		"End position of 5' EST in chromosome"
	string acc3;		"3' EST accession number"
	uint start3;		"Start position of 3' EST in chromosome"
	uint end3;		"End position of 3' EST in chromosome"	
	)
table exoFish
"An evolutionarily conserved region (ecore) with Tetroadon"
   (
   string chrom;      	"Human chromosome or FPC contig"
   uint   chromStart; 	"Start position in chromosome"
   uint   chromEnd;   	"End position in chromosome"
   string name;       	"Ecore name in Genoscope database"
   uint   score;        "Score from 0 to 1000"
   )
table expData
"Expression data (no mapping, just spots)"
    (
    string name; "Name of gene/target/probe etc."
    uint expCount; "Number of scores"
    float[expCount] expScores; "Scores. May be absolute or relative ratio"
    )
table expDistance
"Distance between two genes in expression space."
    (
    string query;	"Name of one gene"
    string target;	"Name of other gene"
    float distance;     "Distance in expression space, always >= 0"
    )
table expRecord
"minimal descriptive data for an experiment in the browser"
(
	uint id; "internal id of experiment"
	string name; "name of experiment"
	lstring description; "description of experiment"
	lstring url; "url relevant to experiment"
	lstring ref; "reference for experiment"
	lstring credit; "who to credit with experiment"
	uint numExtras; "number of extra things"
	lstring[numExtras] extras; "extra things of interest, i.e. classifications"
)
table exprBed
"Expression data information"
    (
    string chrom; "Chromosome name"
    uint chromStart; "position in nucleotides where feature starts on chromosome"
    uint chromEnd; "position in nucleotides where featrure stops on chromsome"
    string name; "feature standardized name; can be a gene, exon or other"
    uint size; "Size of the feature, may be useful if we cannot place it"
    uint uniqueAlign; "1 if alignment was a global maximum, 0 otherwise"
    uint score; "Score from pslLayout of best score"
    string hname; "feature human name: can be a gene, exon or other"
    uint numExp; "Number of experiments in this bed"
    string[numExp] hybes; "Name of experimental hybridization performed, often name of tissue used"
    float[numExp] scores; "log of ratio of feature for particular hybridization"
    )
table fbGene
"Links FlyBase IDs, gene symbols and gene names"
    (
    string geneId;	"FlyBase ID"
    string geneSym;	"Short gene symbol"
    string geneName;	"Gene name - up to a couple of words"
    )

table fbTranscript
"Links FlyBase gene IDs and BDGP transcript IDs"
    (
    string geneId;	"FlyBase Gene ID"
    string transcriptId; "BDGP Transcript ID"
    )

table fbSynonym
"Links all the names we call a gene to it's flybase ID"
    (
    string geneId;	"FlyBase ID"
    string name;	"A name (synonym or real)"
    )

table fbAllele
"The alleles of a gene"
    (
    int id;	"Allele ID"
    string geneId;	"Flybase ID of gene"
    string name;	"Allele name"
    )

table fbRef
"A literature or sometimes database reference"
    (
    int id;	"Reference ID"
    lstring text;	"Usually begins with flybase ref ID, but not always"
    )

table fbRole
"Role of gene in wildType"
    (
    string geneId;	"Flybase Gene ID"
    int fbAllele;	"ID in fbAllele table or 0 if not allele-specific"
    int fbRef;		"ID in fbRef table"
    lstring text;	"Descriptive text"
    )

table fbPhenotype
"Observed phenotype in mutant.  Sometimes contains gene function info"
    (
    string geneId;	"Flybase Gene ID"
    int fbAllele;	"ID in fbAllele table or 0 if not allele-specific"
    int fbRef;		"ID in fbRef table"
    lstring text;	"Descriptive text"
    )
table fishClones
"Describes the positions of fishClones in the assembly"
   (
   string chrom;                   "Human chromosome number"
   uint   chromStart;              "Start position in chromosome"
   uint   chromEnd;                "End position in chromosome"
   string name;                    "Name of clone"
   uint score;                     "Always 1000"
   uint placeCount;                "Number of times FISH'd"
   string[placeCount] bandStarts;  "Start FISH band"
   string[placeCount] bandEnds;    "End FISH band"
   string[placeCount] labs;        "Lab where clone FISH'd"
   string placeType;               "How clone was placed on the sequence assembly"
   uint accCount;                  "Number of accessions associated with the clone"
   string[accCount] accNames;      "Accession associated with clone"
   uint stsCount;                  "Number of STS markers associated with this clone"
   string[stsCount] stsNames;      "Names of STS  markers"
   uint beCount;                   "Number of BAC end sequences associated with this clone"
   string[beCount] beNames;        "Accessions of BAC ends"
   )
table flyBaseSwissProt
"FlyBase acc to SwissProt acc, plus some other SwissProt info"
    (
    string flyBaseId;		"FlyBase FBgn ID"
    string swissProtId;		"SwissProt ID"
    string spGeneName;		"(long) gene name from SwissProt"
    string spSymbol;		"symbolic-looking gene id from SwissProt"
    )
table fosEndPairs
"Positions of end pairs for fosmids"
   (
   short  bin;        "Bin number for browser speedup"
   string chrom;      "Human chromosome"
   uint   chromStart; "Start position of fosmid in chromosome"
   uint   chromEnd;   "End position of fosmid in chromosome"
   string name;       "Name of fosmid"

   uint   score;      "Score = 1000/(# of times fosmid appears in assembly)"
   char[1] strand;    "Value should be + or -"
   string pslTable;   "Table which contains corresponding PSL records for linked features"
   uint   lfCount;    "Number of linked features in the series"
   uint[lfCount]   lfStarts;  "Comma separated list of start positions of each linked feature in genomic"
   uint[lfCount]   lfSizes;   "Comma separated list of sizes of each linked feature in genomic"
   string[lfCount] lfNames;   "Comma separated list of names of linked features"
   )
table bed12
"Browser extensible data"
    (
    string chrom;      "Human chromosome or FPC contig"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint reserved;     "Always zero for now"
    int blockCount;    "Number of blocks"
    int[blockCount] blockSizes; "Comma separated list of block sizes"
    int[blockCount] chromStarts; "Start positions relative to chromStart"
    )
table gbProtAnn
"Protein Annotations from GenPept mat_peptide fields"
    (
    string chrom;      "chromosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    string product;    "Protein product name"
    string note;       "Note (may be empty)"
    string proteinId; "GenBank protein accession(.version)"
    uint giId;        "GenBank db_xref number"
    )
table gcPercent
"Displays GC percentage in 20Kb blocks for genome"
   (
   string chrom;      "Human chromosome number"
   uint   chromStart;  "Start position in genoSeq"
   uint   chromEnd;    "End position in genoSeq"
   string name;       "Constant string GCpct"
   uint   gcPpt;      "GC percentage for 20Kb block"
   )
table genMapDb
"Clones positioned on the assembly by U Penn (V. Cheung)"
    (
    string chrom;	"Chromosome or 'unknown'"
    int chromStart;     "Start position in chrom - negative 1 if unpositioned"
    uint chromEnd;	"End position in chrom"
    string name;	"Name of Clone"
    uint score;	        "Score - always 1000"
    char[1] strand;	"+ or -"

    string accT7; 	"Accession number for T7 BAC end sequence"
    uint startT7;	"Start position in chrom for T7 end sequence"
    uint endT7;		"End position in chrom for T7 end sequence"
    char[1] strandT7;   "+ or -"
    string accSP6; 	"Accession number for SP6 BAC end sequence"
    uint startSP6;	"Start position in chrom for SP6 end sequence"
    uint endSP6;	"End position in chrom for SP6 end sequence"
    char[1] strandSP6;  "+ or -"
    string stsMarker; 	"Name of STS marker found in clone"
    uint stsStart;	"Start position in chrom for STS marker"
    uint stsEnd;	"End position in chrom for STS marker"
    )
table geneBands
"Band locations of known genes"
    (
    string name; "Name of gene - hugo if possible"
    string mrnaAcc; "RefSeq mrna accession"
    int count; "Number of times this maps to the genome"
    string[count] bands; "List of bands this maps to"
    )
table genePred
"A gene prediction."
    (
    string name;	"Name of gene"
    string chrom;	"Chromosome name"
    char[1] strand;     "+ or - for strand"
    uint txStart;	"Transcription start position"
    uint txEnd;         "Transcription end position"
    uint cdsStart;	"Coding region start"
    uint cdsEnd;        "Coding region end"
    uint exonCount;     "Number of exons"
    uint[exonCount] exonStarts; "Exon start positions"
    uint[exonCount] exonEnds;   "Exon end positions"
    )
table genomicDups
"Summary of large genomic Duplications (>1KB >90% similar)"
  (
  string   chrom;       "Human chromosome"
  uint    chromStart;   "Start position in chromosome"
  uint    chromEnd;     "End position in chromosome"
  string  name;         "Other FPC contig involved"
  uint    score;        "Score from 900-1000.  1000 is best"
  char[1] strand;       "Value should be + or -"
  string  otherChrom;   "Human chromosome"
  uint    otherStart;   "Start in other sequence"
  uint    otherEnd;     "End in other sequence"
  uint    alignB;       "bases Aligned"
  uint    matchB;       "aligned bases that match"
  uint    mismatchB;    "aligned bases that do not match"
  float   fracMatch;    "fraction of matching bases"
  float   jcK;          "K-value calculated with Jukes-Cantor"
  )
table genomicSuperDups
"Summary of large genomic Duplications (>1KB >90% similar)"
  (
  string chrom;       "Human chromosome or FPC contig"
  uint chromStart;   "Start position in chromosome"
  uint chromEnd;     "End position in chromosome"
  string name;         "Other chromosome involved"
  uint score;        "Score from 900-1000.  1000 is best"
  char[1] strand;       "Value should be + or -"
  string otherChrom;   "Other Human chromosome or FPC contig"
  uint otherStart;   "Start in other  sequence"
  uint otherEnd;     "End in other  sequence"
  uint otherSize;    "Total size of other sequence"
  uint uid;       "unique id"
  uint posBasesHit;       "HitPositive UnCovered"
  string testResult;       "HitPositive (yes or no) UnCovered (covered=0)"
  string verdict;       "Real or Allele"
  string alignfile;       "alignment file path"
  uint alignL;       "spaces/positions in alignment"
  uint indelN;       "number of indels"
  uint indelS;       "indel spaces"
  uint alignB;       "bases Aligned"
  uint matchB;       "aligned bases that match"
  uint mismatchB;    "aligned bases that do not match"
  uint transitionsB;    "number of transitions"
  uint transversionsB;    "number of transversions"
  float fracMatch;    "fraction of matching bases"
  float fracMatchIndel;    "fraction of matching bases with indels"
  float jcK;          "K-value calculated with Jukes-Cantor"
  float k2K;          "Kimura K"
  )

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
table gl
"Fragment positions in golden path"
    (
    string frag;	"Fragment name"
    uint start;		"Start position in golden path"
    uint end;		"End position in golden path"
    char[1] strand;     "+ or - for strand"
    )
table goa
"GO Association. See http://www.geneontology.org/doc/GO.annotation.html#file"
    (
    string db;	"Database - SPTR for SwissProt"
    string dbObjectId;	"Database accession - like 'Q13448'"
    string dbObjectSymbol;	"Name - like 'CIA1_HUMAN'"
    string notId;  "(Optional) If 'NOT'. Indicates object isn't goId"
    string goId;   "GO ID - like 'GO:0015888'"
    string dbReference; "something like SGD:8789|PMID:2676709"
    string evidence;  "Evidence for association.  Somthing like 'IMP'"
    string withFrom; "(Optional) Database support for evidence I think"
    string aspect;  " P (process), F (function) or C (cellular component)"
    string dbObjectName; "(Optional) multi-word name of gene or product"
    string synonym; "(Optional) field for gene symbol, often like IPI00003084"
    string dbObjectType; "Either gene, transcript, protein, or protein_structure"
    string taxon; "Species (sometimes multiple) in form taxon:9606"
    string date; "Date annotation made in YYYYMMDD format"
    string assignedBy; "Database that made the annotation. Like 'SPTR'"
    )
table goaPart
"Partial GO Association. Useful subset of goa"
    (
    string dbObjectId;	"Database accession - like 'Q13448'"
    string dbObjectSymbol;	"Name - like 'CIA1_HUMAN'"
    string notId;  "(Optional) If 'NOT'. Indicates object isn't goId"
    string goId;   "GO ID - like 'GO:0015888'"
    string aspect;  " P (process), F (function) or C (cellular component)"
    )
table grp
"This describes a group of annotation tracks."
(
string name;    "Group name.  Connects with trackDb.grp"
string label;   "Label to display to user"
float priority; "0 is top"
)

table geneFinder
"A gene finding program."
   (
   uint id;	"Unique ID"
   string name;  "Name of gene finder"
   )

table hgGene
"A gene prediction"
    (
    uint id;          "Unique ID"
    string name;      "Name of gene"
    uint geneFinder;  "Program that made prediction"
    uint startBac;    "Bac this starts in"
    uint startPos;    "Position within bac where starts"
    uint endBac;      "Bac this ends in"
    uint endPos;      "Position withing bac where ends"
    byte orientation; "Orientation relative to start bac"
    uint transcriptCount; "Number of transcripts"
    uint[transcriptCount] transcripts; "Array of transcripts"
    )

table hgTranscript
"A transcript prediction"
    (
    uint id;          "Unique ID"
    string name;      "Name of transcript"
    uint hgGene;        "Gene this is in"
    uint startBac;    "Bac this starts in"
    uint startPos;    "Position within bac where starts"
    uint endBac;      "Bac this ends in"
    uint endPos;      "Position withing bac where ends"
    uint cdsStartBac; "Start of coding region."
    uint cdsStartPos; "Start of coding region."
    uint cdsEndBac;   "End of coding region."
    uint cdsEndPos;   "End of coding region."
    byte orientation; "Orientation relative to start bac"
    uint exonCount;   "Number of exons"
    uint[exonCount] exonStartBacs;  "Exon start positions"
    uint[exonCount] exonStartPos;  "Exon start positions"
    uint[exonCount] exonEndBacs;  "Exon start positions"
    uint[exonCount] exonEndPos;  "Exon start positions"
    )
table humanParalog
"Human Paralog"
(
string chrom;       "Human Chrom"
uint chromStart;       "Start on Human"
uint chromEnd;         "End on Human"
string name;   "Human Gene Prediction - paralog"
uint score;         "Human mouse aligment score using Blat"
string strand;      "Strand on Human"
uint thickStart;       "Start on Human"
uint thickEnd;         "End on Human"
)

table idName
"Map a numerical ID to a string"
    (
    int id;	"Numerical id"
    string name; "Associated string"
    )

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
table isochores
"Describes the positions of isochores with a chromosome"
   (
   string chrom;       "Human chromosome number"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;       "Name of isochore - AT-rich or GC-rich"

   uint   gcPpt;      "GC content in parts per thousand"
   )
table jaxOrtholog
"Jackson Labs Mouse Orthologs"
    (
    string humanSymbol;	"Human HUGO symbol"
    string humanBand; "Human chromosomal location"
    string mgiId;     "Mouse database id"
    string mouseSymbol; "Mouse human symbol"
    string mouseChr; "Mouse chromosome"
    string mouseCm; "Mouse genetic map positionin centimorgans"
    string mouseBand; "Mouse chromosome band if any"
    )
table jaxQTL
"Quantitative Trait Loci from Jackson Labs / Mouse Genome Informatics"
    (
    string chrom;      "Human chromosome or FPC contig"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000 (bed6 compat.)"
    char[1] strand;    "+ or - (bed6 compat.)"
    string marker;     "MIT SSLP Marker w/highest correlation"
    string mgiID;      "MGI ID"
    string description; "MGI description"
    float  cMscore;    "cM position of marker associated with peak LOD score"
    )
table jaxQTL2
"Quantitative Trait Loci from Jackson Labs / Mouse Genome Informatics"
    (
    string chrom;      "chromosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000 (bed6 compat.)"
    char[1] strand;    "+ or - (bed6 compat.)"
    uint   thickStart; "start of thick region"
    uint   thickEnd; "start of thick region"
    string marker;     "MIT SSLP Marker w/highest correlation"
    string mgiID;      "MGI ID"
    string description; "MGI description"
    float  cMscore;    "cM position of marker associated with peak LOD score"
    )
table keggMapDesc
"Description of KEGG pathway map"
   (
   string mapID; 	"KEGG pathway map ID"
   string description; 	"KEGG pathway map description"
   )
table keggPathway
"KEGG pathway cross reference"
   (
   string kgID; 	"Known Gene ID"
   string locusID; 	"LocusLink ID"
   string mapID; 	"KEGG pathway map ID"
   )
table kgAlias
"Link together a Known Gene ID and a gene alias"
    (
    string kgID;        "Known Gene ID"
    string alias;	"a gene alias"
    )
table kgProtAlias
"Link together a Known Gene ID and a protein alias"
    (
    string kgID;        	"Known Gene ID"
    string displayID;		"protein display ID"
    string alias;		"a protein alias"
    )
table kgXref
"Link together a Known Gene ID and a gene alias"
    (
    string kgID;        "Known Gene ID"
    string mRNA;        "mRNA ID"
    string spID;        "SWISS-PROT protein Accession number"
    string spDisplayID; "SWISS-PROT display ID"
    string geneSymbol;  "Gene Symbol"
    string refseq;      "RefSeq ID"
    string protAcc;     "NCBI protein Accession number"
    string description; "Description"
    )
table knownCanonical
"Describes the canonical splice variant of a gene"
    (
    string chom;	"Chromosome"
    int chromStart;	"Start position (0 based). Corresponds to txStart"
    int chromEnd;	"End position (non-inclusive). Corresponds to txEnd"
    int clusterId;	"Which cluster of transcripts this belongs to in knownIsoforms"
    string transcript;	"Corresponds to knownGene name field."
    string protein;	"SwissProt ID of associated protein."
    )
table knownGeneLink 
"Known Genes Link table, currently storing DNA based entries only."
   (
   string name; 	"Known Gene ID"
   char[1] seqType;	"sequence type, 'g' genomic, 'm' mRNA, etc."
   string proteinID; 	"Protein ID"
   )
table knownInfo
"Auxiliary info about a known gene"
    (
    string name;     "connects with genieKnown->name"
    string transId;  "Transcript id. Genie generated ID."
    string geneId;   "Gene (not trascript) id"
    uint geneName;   "Connect to geneName table"
    uint productName;  "Connects to productName table"
    string proteinId;  "Genbank accession of protein?"
    string ngi;        "Genbank gi of nucleotide seq."
    string pgi;        "Genbank gi of protein seq."
    )
table knownIsoforms
"Links together various transcripts of a gene into a cluster"
    (
    int clusterId;	"Unique id for transcript cluster (aka gene)"
    string transcript;  "Corresponds to name in knownGene table, transcript in knownCanonical"
    )
table knownMore
"Lots of auxiliary info about a known gene"
    (
    string name;        "The name displayed in the browser: OMIM, gbGeneName, or transId"
    string transId;     "Transcript id. Genie generated ID."
    string geneId;      "Gene (not transcript) Genie ID"
    uint gbGeneName;      "Connect to geneName table. Genbank gene name"
    uint gbProductName;   "Connects to productName table. Genbank product name"
    string gbProteinAcc;  "Genbank accession of protein"
    string gbNgi;         "Genbank gi of nucleotide seq."
    string gbPgi;         "Genbank gi of protein seq."
    uint omimId;          "OMIM ID or 0 if none"
    string omimName;	  "OMIM primary name"
    uint hugoId;          "HUGO Nomeclature Committee ID or 0 if none"
    string hugoSymbol;    "HUGO short name"
    string hugoName;	  "HUGO descriptive name"
    string hugoMap;       "HUGO Map position"
    uint pmId1;           "I have no idea - grabbed from a HUGO nomeids.txt"
    uint pmId2;           "Likewise, I have no idea"
    string refSeqAcc;	  "Accession of RefSeq mRNA"
    string aliases;	  "Aliases if any.  Comma and space separated list"
    uint locusLinkId;     "Locus link ID"
    string gdbId;	  "NCBI GDB database ID"
    )
table knownTo
"Map known gene to some other id"
    (
    string name;	"Same as name field in known gene"
    string value;       "Other id"
    )
table knownToSuper
"Map protein superfamilies to known genes"
    (
    string gene;	"Known gene ID"
    int superfamily;	"Superfamily ID"
    int start;	"Start of superfamily domain in protein (0 based)"
    int end;	"End (noninclusive) of superfamily domain"
    float eVal;	"E value of superfamily assignment"
    )
table lfs
"Standard linked features series table"
   (
   short  bin;        "Bin number for browser speedup"
   string chrom;      "Human chromosome or FPC contig"
   uint   chromStart; "Start position of clone in chromosome"
   uint   chromEnd;   "End position of clone in chromosome"
   string name;       "Name of clone"

   uint   score;      "Score = 1000/(# of times clone appears in assembly)"
   char[1] strand;    "Value should be + or -"
   string pslTable;   "Table which contains corresponding PSL records for linked features"
   uint   lfCount;    "Number of linked features in the series"
   uint[lfCount]   lfStarts;  "Comma separated list of start positions of each linked feature in genomic"
   uint[lfCount]   lfSizes;   "Comma separated list of sizes of each linked feature in genomic"
   string[lfCount] lfNames;   "Comma separated list of names of linked features"
   )
table llaInfo
"Extra information for a lowe lab array"
    (
    string name;    "Name of primer pair"
    string type;    "Type of primer pair (ORF or intergenic)"
    float SnTm;     "Sense primer annealing temp"
    float SnGc;     "Sense primer annealing temp"
    float SnSc;     "Sense primer self-complementarity score"
    float Sn3pSc;   "Sense primer 3-prime self-complementarity score"
    float AsnTm;    "Antisense primer annealing temp"
    float AsnGc;    "Antisense primer annealing temp"
    float AsnSc;    "Antisense primer self-complementarity score"
    float Asn3pSc;  "Antisense primer 3-prime self-complementarity score"
    uint prodLen;   "PCR product length"
    uint ORFLen;    "Source ORF length"
    float meltTm;   "PCR melting temperature"
    float frcc;     "Forward+reverse primer cross complementarity"
    float fr3pcc;   "3-prime forward+reverse primer cross complementarity"
    lstring SnSeq;  "Sense primer sequence"
    lstring AsnSeq; "Antisense primer sequence"
    uint numCorrs;  "Number of correlations stored"
    string[numCorrs] corrNames; "Names of correlated genes"
    float[numCorrs] corrs;  "Correlations"
    )
table loweTrnaGene
"Information for a tRNA gene annotated by Todd Lowe"
  (
  string chrom;       "Human chromosome or FPC contig"
  uint chromStart;   "Start position in chromosome"
  uint chromEnd;     "End position in chromosome"
  string name;         "Other chromosome involved"
  uint score;        "Score from 900-1000.  1000 is best"
  char[1] strand;       "Value should be + or -"
  string aa;         "Amino acid for the tRNA"
  string ac;         "Anticodon for the tRNA"
  char[1] ci;        "Contains intron or not: Y or N"
  float scan;        "tRNAScanSE score"
  )
table mapSts
"An STS based map in relationship to golden path"
(
string chrom;	"Chromosome"
uint chromStart; "Start position in chrom"
uint chromEnd;	"End position in chrom"
string name;	"Name of STS marker"

uint score;	"Score of a marker - depends on how many contigs it hits"

uint identNo;	"Identification number of STS"
string rhChrom;	"Chromosome number from the RH map - 0 if there is none"
float distance;	"Distance from the RH map"
string ctgAcc;	"Contig accession number"
string otherAcc;	"Accession number of other contigs that the marker hits"
)
table mcnBreakpoints
"Chromosomal breakpoints from the MCN Project"
        (
        string chrom;    "Human chromosome number"
        uint   chromStart;  "Start position in genoSeq"
        uint   chromEnd;    "End position in genoSeq"
        string name;       "Shortened Traitgroup Name"
        uint score;     "Always 1000 for now"
	string caseId;	"MCN Case ID"
	string bpId;	"MCN Breakpoint ID"
	string trId;	"MCN Trait ID"
	string trTxt;   "MCN Trait name"
	string tgId;	"MCN Traitgroup ID"
	string tgTxt;	"MCN Traitgroup Name"
   )
table MGIid
"MGI to Locus Link table"
(
char[8]		LLid;		    "Locus Link ID"
char[12] 	MGIid;              "MGI ID"
char[32]	symbol;	    		"symbol"
)
table minGeneInfo
"Auxilliary info about a gene (less than the knownInfo)"
    (
    string name;     "gene name"
    string product;  "gene product"
    string note;     "gene note"
    )
table mouseOrtho
"Human Mouse Orthologs"
(
string chrom;       "Human Chrom"
uint chromStart;       "Start on Human"
uint chromEnd;         "End on Human"
string name;   "Mouse Gene Prediction"
uint score;         "Human mouse aligment score using Blat"
string strand;      "Strand on Human"
uint thickStart;       "Start on Human"
uint thickEnd;         "End on Human"
)

table mouseSyn
"Synteny between mouse and human chromosomes."
    (
    string chrom;	 "Name of chromosome"
    uint chromStart;	 "Start in chromosome"
    uint chromEnd;       "End in chromosome"
    string name;         "Name of mouse chromosome"
    int segment;         "Number of segment"
    )
table mouseSynWhd
"Whitehead Synteny between mouse and human chromosomes (bed 6 +)."
    (
    string chrom;        "Human chromosome"
    uint   chromStart;   "Start position in human chromosome"
    uint   chromEnd;     "End position in human chromosome"
    string name;         "Name of mouse chromosome"
    uint   score;        "Unused (bed 6 compatibility)"
    char[1] strand;      "+ or -"
    uint   mouseStart;   "Start position in mouse chromosome"
    uint   mouseEnd;     "End position in mouse chromosome"
    string segLabel;     "Whitehead segment label"
    )
table mrnaRefseq
"Cross reference table between refseq and mRNA IDs based on LocusLink"
   (
   string mrna; 	"mRNA ID"
   string refseq;	"RefSeq ID"
   )
table mysqlTableStatus
"Table status info from MySQL.  Just includes stuff common to 3.1 & 4.1"
    (
    String name;	 "Name of table"
    String type;	 "Type - MyISAM, InnoDB, etc."
    String rowFormat;	 "Row storage format: Fixed, Dynamic, Compressed"
    int rowCount;        "Number of rows"
    int averageRowLength;"Average row length"
    int dataLength;	 "Length of data file"
    int maxDataLength;   "Maximum length of data file"
    int indexLength;	 "Length of index file"
    int dataFree;	 "Number of allocated but not used bytes of data"
    String autoIncrement; "Next autoincrement value (or NULL)"
    String createTime;	 "Table creation time"
    String updateTime;	 "Table last update time"
    String checkTime;    "Table last checked time (or NULL)"
    )
table netAlign
"Database representation of a net of alignments."
(
uint level;       "Level of alignment"
string tName;     "Target chromosome"
uint tStart;      "Start on target"
uint tEnd;        "End on target"
char[1] strand;   "Orientation of query. + or -"
string qName;     "Query chromosome"
uint qStart;      "Start on query"
uint qEnd;        "End on query"
uint chainId;     "Associated chain Id with alignment details"
uint ali;         "Bases in gap-free alignments"
double score;     "Score - a number proportional to 100x matching bases"
int qOver;        "Overlap with parent gap on query side. -1 for undefined"
int qFar;         "Distance from parent gap on query side. -1 for undefined"
int qDup;         "Bases with two or more copies in query. -1 for undefined"
string type;      "Syntenic type: gap/top/syn/nonsyn/inv"
int tN;           "Unsequenced bases on target. -1 for undefined"
int qN;           "Unsequenced bases on query. -1 for undefined"
int tR;           "RepeatMasker bases on target. -1 for undefined"
int qR;           "RepeatMasker bases on query. -1 for undefined"
int tNewR;        "Lineage specific repeats on target. -1 for undefined"
int qNewR;        "Lineage specific repeats on query. -1 for undefined"
int tOldR;        "Bases of ancient repeats on target. -1 for undefined"
int qOldR;        "Bases of ancient repeats on query. -1 for undefined"
int tTrf;         "Bases of Tandam repeats on target. -1 for undefined"
int qTrf;         "Bases of Tandam repeats on query. -1 for undefined"
)
table pbAaDistX
"Distribution for a specific amino acid X"
   (
   float x; "x value"
   float y; "count"
   )
table pbAnomLimit 
"Protein Aminon Acid Anomaly limits for each AA"
   (
   char[1] AA;		"Amonio Acid"
   float   pctLow;	"percentage (100%=1.0) Lower  bound"
   float   pctHi;	"percentage (100%=1.0) Higher bound"
   )
table pbResAvgStd
"Residue average and standard deviation"
   (
   char[1] residue;	"protein residue"
   float   avg;		"average"
   float   stddev;	"Standard deviation"
   )
table pbStamp
"Info needed for a Proteom Browser Stamp"
    (
    char[40] stampName;	"Short Name of stamp"
    char[40] stampTable;"Database table name of the stamp (distribution) data"
    char[40] stampTitle;"Stamp Title to be displayed"
    int len;		"number of x-y pairs"
    float  xmin;	"x minimum"
    float  xmax;	"x maximum"
    float  ymin;	"y minimum"
    float  ymax;	"y maximum"
    string stampDesc;	"Description of the stamp"
    )
table pepCCntDist
"Cysteine count Distribution"
   (
   float x;		"number of Cysteines"
   float y;		"count of proteins with x Cysteines"
   )
table pepExonCntDist
"Exon count Distribution"
   (
   float x;		"number of exon"
   float y;		"count of proteins with x exons"
   )
table pepHydroDist
"Hydrophobicity Distribution"
   (
   float x;		"Hydrophobicity value"
   float y;		"count of proteins with hydrophobicity near x"
   )
table pepIPCntDist
"InterProt domain count Distribution"
   (
   float x;		"number of InterPro domains"
   float y;		"count of proteins with x InterPro domains"
   )
table pepMolWtDist
"Molecular Wight Distribution"
   (
   float x;		"Molecular weight value"
   float y;		"count of proteins with molecular weight near x"
   )
table pepMwAa 
"Molecular weight and AA length of proteins"
   (
   string accession; 	"SWISS-PROT protein accession number"
   float  molWeight; 	"Molecular weight"
   int    aaLen; 	"Length of protein sequence"
   )
table pepPiDist
"pI Distribution"
   (
   float x;		"pI  value"
   float y;		"count of proteins with pI value near x"
   )
table pepPred
"A predicted peptide - linked to a predicted gene."
    (
    string name;	"Name of gene - same as in genePred"
    lstring seq;        "Peptide sequence"
    )
table pepResDist
"Residue Distribution"
   (
   float x;		"Residue index (WCMHYNFIDQKRTVPGEASL) "
   float y;		"count of proteins with residue index of x"
   )
table pileups
"Pileup locations and hit count min/avg/max."
    (
    string chrom;      "Chromosome."
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name (pileup location suitable for browser position)"
    uint   score;      "Int score (bed compat) -- use average by default."
    uint   pMin;       "Minimum hit count of any base in this pileup."
    float  pAvg;       "Average hit count for all bases in this pileup."
    uint   pMax;       "Maximum hit count of any base in this pileup."
    )
table plasEndPairs
"Standard linked features series table"
   (
   short  bin;        "Bin number for browser speedup"
   string chrom;      "Human chromosome or FPC contig"
   uint   chromStart; "Start position of clone in chromosome"
   uint   chromEnd;   "End position of clone in chromosome"
   string name;       "Name of clone"

   uint   score;      "Score = 1000/(# of times clone appears in assembly)"
   char[1] strand;    "Value should be + or -"
   string pslTable;   "Table which contains corresponding PSL records for linked features"
   uint   lfCount;    "Number of linked features in the series"
   uint[lfCount]   lfStarts;  "Comma separated list of start positions of each linked feature in genomic"
   uint[lfCount]   lfSizes;   "Comma separated list of sizes of each linked feature in genomic"
   string[lfCount] lfNames;   "Comma separated list of names of linked features"
   )
table pseudoGeneLink
"links a gene/pseudogene prediction to an ortholog or paralog."
    (
    string chrom;	"Chromosome name for pseudogene"
    uint chromStart;	"pseudogene alignment start position"
    uint chromEnd;      "pseudogene alignment end position"
    string name;        "Name of pseudogene"
    uint score;         "score of pseudogene with gene"
    string strand;      "strand of pseudoegene"
    string assembly;	"assembly for gene"
    string geneTable;	"mysql table of gene"
    string gene;	"Name of gene"
    string gChrom;	"Chromosome name"
    uint gStart;	"gene alignment start position"
    uint gEnd;          "gene alignment end position"
    string gStrand;     "strand of gene"
    uint exonCount;     "# of exons in gene "
    uint geneOverlap;   "bases overlapping"
    uint polyA;         "length of polyA"
    uint polyAstart;    "start f polyA"
    uint exonCover;     "number of exons in Gene covered"
    uint intronCount;   "number of introns in pseudogene"
    uint bestAliCount;  "number of good mrnas aligning"
    uint matches;       "matches + repMatches"
    uint qSize;         "aligning bases in pseudogene"
    uint qEnd;          "end of cdna alignment"
    uint tReps;         "repeats in gene"
    uint qReps;         "repeats in pseudogene"
    uint overlapDiag;   "bases on the diagonal to mouse"
    uint coverage;      "bases on the diagonal to mouse"
    uint label;         "1=pseudogene,-1 not pseudogene"
    uint milliBad;      "milliBad score, pseudogene aligned to genome"
    uint chainId;       "chain id of gene/pseudogene alignment"
    string refSeq;	"Name of closest regSeq to gene"
    uint rStart;	"refSeq alignment start position"
    uint rEnd;          "refSeq alignment end position"
    string mgc;	        "Name of closest mgc to gene"
    uint mStart;	"mgc alignment start position"
    uint mEnd;          "mgc alignment end position"
    string kgName;	"Name of closest knownGene to gene"
    uint kStart;	"kg alignment start position"
    uint kEnd;          "kg alignment end position"
    uint kgId;          "kg id"
    )
table pslWScore
"Summary info about a patSpace alignment with a score addition"
    (
    uint    match;      "Number of bases that match that aren't repeats"
    uint    misMatch;   "Number of bases that don't match"
    uint    repMatch;   "Number of bases that match but are part of repeats"
    uint    nCount;       "Number of 'N' bases"
    uint    qNumInsert;   "Number of inserts in query"
    int     qBaseInsert;  "Number of bases inserted in query"
    uint    tNumInsert;   "Number of inserts in target"
    int     tBaseInsert;  "Number of bases inserted in target"
    char[2] strand;       "+ or - for query strand. For mouse second +/- for genomic strand"
    string  qName;        "Query sequence name"
    uint    qSize;        "Query sequence size"
    uint    qStart;       "Alignment start position in query"
    uint    qEnd;         "Alignment end position in query"
    string  tName;        "Target sequence name"
    uint    tSize;        "Target sequence size"
    uint    tStart;       "Alignment start position in target"
    uint    tEnd;         "Alignment end position in target"
    uint    blockCount;   "Number of blocks in alignment"
    uint[blockCount] blockSizes;  "Size of each block"
    uint[blockCount] qStarts;     "Start of each block in query."
    uint[blockCount] tStarts;     "Start of each block in target."
    float     score;         "score field"
    )
table recombRate
"Describes the recombination rate in 1Mb intervals based on deCODE map"
   (
   string chrom;    "Human chromosome number"
   uint   chromStart;  "Start position in genoSeq"
   uint   chromEnd;    "End position in genoSeq"
   string name;       "Constant string recombRate"

   float   decodeAvg;    "Calculated deCODE recombination rate"
   float   decodeFemale; "Calculated deCODE female recombination rate"
   float   decodeMale;   "Calculated deCODE male recombination rate"
   float   marshfieldAvg;    "Calculated Marshfield recombination rate"
   float   marshfieldFemale; "Calculated Marshfield female recombination rate"
   float   marshfieldMale;   "Calculated Marshfield male recombination rate"
   float   genethonAvg;    "Calculated Genethon recombination rate"
   float   genethonFemale; "Calculated Genethon female recombination rate"
   float   genethonMale;   "Calculated Genethon male recombination rate"
   )
table refLink
"Link together a refseq mRNA and other stuff"
    (
    string name;        "Name displayed in UI"
    string product;	"Name of protein product"
    string mrnaAcc;	"mRNA accession"
    string protAcc;	"protein accession"
    uint geneName;	"pointer to geneName table"
    uint prodName;	"pointer to prodName table"
    uint locusLinkId;	"Locus Link ID"
    uint omimId;	"OMIM ID"
    )
table refSeqStatus
"RefSeq Gene Status."
    (
    string mrnaAcc;	"RefSeq gene accession name"
    string status;	"Status (Reviewed, Provisional, Predicted)"
    )
table rikenBest
"The best Riken mRNA in each cluster"
    (
    string name;	"Riken mRNA ID"
    float orfScore; "Score from bestorf - 50 is decent"
    char[1] orfStrand; "Strand bestorf is on: +, - or ."
    int intronOrientation;  "+1 for each GT/AG intron, -1 for each CT/AC"
    string position;	"Position in genome of cluster chrN:start-end format"
    int rikenCount;	"Number of Riken mRNAs in cluster"
    int genBankCount;	"Number of Genbank mRNAs in cluster"
    int refSeqCount;    "Number of RefSeq mRNAs in cluster"
    string clusterId;	"ID of cluster"
    )
table rikenUcscCluster
"A cluster of Riken (and other) mRNAs"
    (
    string chrom;	"Chomosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Always 1000"
    char[1] strand;    "+ or -"
    uint refSeqCount;	"Number of refSeq mrnas in cluster"
    string[refSeqCount] refSeqs;	"List of refSeq accessions"
    uint genBankCount;	"Number of Genbank mrnas in cluster"
    string[genBankCount] genBanks;	"List of Genbank accessions"
    uint rikenCount;	"Number of Riken mrnas in cluster"
    string[rikenCount] rikens;		"List of Riken IDs"
    )
table rmskOut
"RepeatMasker .out record"
    (
    uint swScore;       "Smith Waterman alignment score"
    uint milliDiv;      "Base mismatches in parts per thousand"
    uint milliDel;      "Bases deleted in parts per thousand"
    uint milliIns;      "Bases inserted in parts per thousand"
    string genoName;    "Genomic sequence name"
    uint genoStart;     "Start in genomic sequence"
    uint genoEnd;       "End in genomic sequence"
    int  genoLeft;      "Size left in genomic sequence"
    char[1] strand;     "Relative orientation + or -"
    string repName;     "Name of repeat"
    string repClass;    "Class of repeat"
    string repFamily;   "Family of repeat"
    int repStart;       "Start in repeat sequence"
    uint repEnd;        "End in repeat sequence"
    int repLeft;        "Size left in repeat sequence"
    char[1] id;         "'*' or ' '.  I don't know what this means"
    )
table rnaFold
"Info about folding of RNA into secondary structure"
    (
    string name;	"mRNA accession"
    lstring seq;	"mRNA sequence (U's instead of T's)"
    lstring fold;	"Parenthesis and .'s that describe folding"
    float energy;	"Estimated free energy of folding (negative)"
    )
table rnaGene
"Describes functional RNA genes."
    (
    string chrom;	"Chromosome gene is on"
    uint chromStart;    "Start position in chromosome"
    uint chromEnd;      "End position in chromosome"
    string name;        "Name of gene"
    uint score;         "Score from 0 to 1000"
    char[1] strand;     "Strand: + or -"
    string source;      "Source as in Sean Eddy's files."
    string type;        "Type - snRNA, rRNA, tRNA, etc."
    float fullScore;    "Score as in Sean Eddy's files."
    ubyte isPsuedo;      "TRUE(1) if psuedo, FALSE(0) otherwise"
    )
table rnaGroup
"Details of grouping of rna in cluster"
    (
    string chrom;	"Chomosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Always 1000"
    char[1] strand;    "+ or -"
    uint refSeqCount;	"Number of refSeq mrnas in cluster"
    string[refSeqCount] refSeqs;	"List of refSeq accessions"
    uint genBankCount;	"Number of Genbank mrnas in cluster"
    string[genBankCount] genBanks;	"List of Genbank accessions"
    uint rikenCount;	"Number of Riken mrnas in cluster"
    string[rikenCount] rikens;		"List of Riken IDs"
    )
table roughAli
"A rough alignment - not detailed"
   (
   string chrom;      	"Human chromosome or FPC contig"
   uint   chromStart; 	"Start position in chromosome"
   uint   chromEnd;   	"End position in chromosome"
   string name;       	"Name of other sequence"
   uint   score;        "Score from 0 to 1000"
   char[1] strand;      "+ or -"
   uint otherStart;	"Start in other sequence"
   uint otherEnd;	"End in other sequence"
   )

table sage
"stores sage data in terms of uni-gene identifiers."
(
	int uni; "Number portion of uni-gene identifier, add 'Hs.' for full identifier."
	char[64] gb; "Genebank accesion number."
	char[64] gi; "gi field in unigene descriptions."
	lstring description; "Description from uni-gene fasta headers."
	int numTags; "Number of tags."
	string[numTags] tags; "Tags for this unigene sequence."
	int numExps; "Number of experiments."
	int[numExps] exps; "index of experiments in order of aves and stdevs."
	float[numExps] meds; "The median count of all tags for each experiment."
	float[numExps] aves; "The average count of all tags for each experiment."
	float[numExps] stdevs; "Standard deviation of all counts for each experiment"
)

table sageExp
"data related to sage Experiments, tissued descriptions, etc."
(
	int num; "Index of the experiment in the sage table."
	string exp; "experiments name."
	int totalCount; "Sum of all the tag counts for this experiment."
	string tissueType; "Brief Description of cells used."
	string tissueDesc; "Brief Description of tissues."
	string tissueSupplier; "Who supplied the tissue."
	string organism; "organism source of cells."
	string organ; "organ identifier."
	lstring producer; "source of tissue."
	lstring desription; "description of experiment."
)
table sample
"Any track that has samples to display as y-values (first 6 fields are bed6)"
(
string chrom;         "Human chromosome or FPC contig"
uint chromStart;      "Start position in chromosome"
uint chromEnd;        "End position in chromosome"
string name;          "Name of item"
uint   score;         "Score from 0-1000"
char[2] strand;        "# + or -"

uint sampleCount;                   "number of samples total"
uint[sampleCount] samplePosition;   "bases relative to chromStart (x-values)"
int[sampleCount] sampleHeight;     "the height each pixel is drawn to [0,1000]"
)

table sanger22extra
"Table with additional information about a Sanger 22 gene"
    (
    string name;	"Transcript name"
    string locus;	"Possibly biological short name"
    lstring description; "Description from Sanger gene GFFs"
    string geneType;	"Type field from Sanger gene GFFs"
    string cdsType;	"Type field from Sanger CDS GFFs"
    )
table scopDes
"Structural Classification of Proteins Description. See Lo Conte, Brenner et al. NAR 2002"
    (
    int sunid;	"Unique integer"
    char[2] type;	"Type.  sf=superfamily, fa = family, etc."
    string sccs;	"Dense hierarchy info."
    string sid;		"Older ID."
    lstring description; "Descriptive text."
    )
table scoredRef
"A score, a range of positions in the genome and a extFile offset"
   (
   string chrom;      	"Chromosome (this species)"
   uint   chromStart; 	"Start position in chromosome (forward strand)"
   uint   chromEnd;   	"End position in chromosome"
   uint   extFile;	"Pointer to associated external file"
   bigint offset;	"Offset in external file"
   float score;		"Overall score"
   )


table sfAssign
"Superfamily Assignment table"
  (
  string genomeID;	"genome ID"
  string seqID;		"sequence ID"
  string modelID;	"model ID"
  string region;	"region"
  string eValue;	"E value"
  string sfID;		"Superfamily entry ID"
  string sfDesc;	"Superfamily entry description"
  )
table sfDes 
"Superfamily Description table"
  (
  int id;			"ID"
  char[2] level;		"level"
  string classification;	"classification"
  string name;			"name"
  string description;		"description"
  )
table sfDescription
"Summary entry description"
  (
  string name;		"Ensembl transcription ID"
  string proteinID;	"Ensembl protein ID"
  string description;	"Description"
  )
table sgdAbundance
"Protein abundance data from http://yeastgfp.ucsf.edu via SGD"
    (
    string name;	"ORF name in sgdGene table"
    float abundance;	"Absolute abundance from 41 to 1590000"
    string error;	"Error - either a floating point number or blank"
    )
table sgdClone
"Clone info from yeast genome database"
    (
    string chrom;      	"Chromosome in chrN format"
    uint   chromStart; 	"Start position in chromosome"
    uint   chromEnd;   	"End position in chromosome"
    string name;       	"Washington University Name"
    string atccName;	"ATCC clone name (optional)"
    )
table sgdDescription
"Description of SGD Genes and Other Features"
    (
    string  name;	"Name in sgdGene or sgdOther table"
    string type;	"Type of feature from gff3 file"
    lstring description; "Description of feature"
    )
table sgdOther
"Features other than coding genes from yeast genome database"
    (
    string chrom;	"Chromosome in chrNN format"
    int chromStart;	"Start (zero based)"
    int chromEnd;	"End (non-inclusive)"
    string name;	"Feature name"
    int score;		"Always 0"
    char[1] strand;	"Strand: +, - or ."
    string type;	"Feature type"
    )
table simpleNucDiff
"Simple nucleotide difference"
    (
    string chrom;      "Target species chromosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string tSeq;	"Sequence in target species"
    string qSeq;	"Sequence in other (query) species"
    )
table simpleRepeat
"Describes the Simple Tandem Repeats"
   (
   string chrom;      	"Human chromosome or FPC contig"
   uint   chromStart; 	"Start position in chromosome"
   uint   chromEnd;   	"End position in chromosome"
   string name;       	"Simple Repeats tag name"
   uint   period;     	"Length of repeat unit"
   float  copyNum;    	"Mean number of copies of repeat"
   uint   consensusSize;	"Length of consensus sequence"
   uint   perMatch;  	"Percentage Match"
   uint   perIndel;  	"Percentage Indel"
   uint   score; 	"Score between  and .  Best is ."
   uint   A;  	"Number of A's in repeat unit"
   uint   C;  	"Number of C's in repeat unit"
   uint   G;  	"Number of G's in repeat unit"
   uint   T;  	"Number of T's in repeat unit"
   float   entropy;  	"Entropy"
   lstring sequence;    	"Sequence of repeat unit element"
   )
table snp
"Single nucleotide polymorphisms"
    (
    string chrom;      "Human chromosome or FPC contig"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of SNP"
    )
table snpMap
"SNP positions from various sources"
    (
    string chrom;	"Chromosome or 'unknown'"
    uint   chromStart;  "Start position in chrom"
    uint   chromEnd;	"End position in chrom"
    string name;	"Name of SNP - rsId or Affy name"
    string source;	"BAC_OVERLAP | MIXED | RANDOM | OTHER | Affy10K | Affy120K"
    string type;        "SNP | INDEL | SEGMENTAL"
    )
table softPromoter
"Softberry's Promoter Predictions"
    (
    string chrom;      	"Human chromosome or FPC contig"
    uint   chromStart; 	"Start position in chromosome"
    uint   chromEnd;   	"End position in chromosome"
    string name;       	"As displayed in browser"
    uint   score;        "Score from 0 to 1000"
    string type;	"TATA+ or TATAless currently"
    float origScore;    "Score in original file, not scaled"
    string origName;    "Name in original file"
    string blockString; "From original file.  I'm not sure how to interpret."
    )
table softberryHom
"Protein homologies behind Softberry genes"
    (
    string name;	"Softberry gene name"
    string giString;	"String with Genbank gi and accession"
    lstring description; "Freeform (except for no tabs) description"
    )
table spMrna
"The best representative mRNA for a protein"
   (
   string spID;		"SWISS-PROT display ID"
   string mrnaID;	"GenBank mRNA accession"
   )

table splignAlign
"NCBI tab-seperated alignment file"
    (
    string query;       "query name"
    string target;      "target name"
    float perId;        "percent identity"
    int match;          "number of match bases"
    int qStart;		"query start"
    int qEnd;		"query end"
    int tStart; 	"target Start"
    int tEnd;		"target End"
    string type;	"type of blocks"
    string anotation;	"anotation of misMatch, indel"
    )

table spXref2
"A xref table between SWISS-PROT ids and other databases."
	(
	string accession;	"SWISS-PROT accession number"
	string displayID;	"SWISS-PROT display ID"
	string division;	"SWISS-PROT division"
	string extDB;		"external database"
	string extAC;		"externa Accession number"
	int    bioentryID;	"biosql bioentry ID"
	int    biodatabaseID;   "biosql biodatabase ID"
	)
table stanMad 
"stanford's microarray database output"
(
string exp; "Name of the experiment based on the array id."
lstring name; "Name of the gene or sample that is at a given position."
string type; "Indicates whether an element is a control or cDNA."
int ch1i; "The average total signal for each spot for Cy3"
int ch1b; "The local background calculated for each spot for Cy3"
int ch1d; "The difference between the total and background for each element."
int ch2i; "The average total signal for each spot for Cy5."
int ch2b; "The local background calculated for each spot for Cy5"
int ch2d; "The difference between the total and background for each element."
int ch2in; "The average total signal for each spot for Cy5 normalized so that the average log ratio of all well measured spots is equal to 0."
int ch2bn; "The local background calculated for each spot for Cy5 normalized so that the average log ratio of all well measured spots is equal to 0."
int ch2dn; "The difference between the total and background for each element normalized so that the average log ratio of all well measured spots is equal to 0."
float rat1; "CH1D/CH2D"
float rat2; "CH2D/CH1D"
float rat1n; "CH1D/CH2DN"
float rat2n; "CH2DN/CH1D"
float mrat; "unknown"
float crt1; "and CRT2 Unused."
float crt2; "unused"
float regr; "The ratio estimated by the slope of the line fit to the distribution of pixels in each spot."
float corr; "The correlation of the signal at each pixel of a spot in CH1 to CH2."
float edge; "unnknown"
int fing; "unknown"
int grid; "unknown"
int arow; "unknown"
int row; "unknown"
int acol; "unknown"
int col; "unknown"
int plat; "The plate ID from which a sample was printed."
string prow; "The plate row from which a sample was printed."
int pcol; "The plate column from which a sample was printed."
int flag; "A user defined field which identifies spots which are visually of low quality. The value 0 is good all other values represent poor quality spots."
int clid; "The Identification number of the printed cDNA clone. These are mostly IMAGE clones. It is recommended to verify the clone identity by GenBank lookup using the EST accession number (see above)"
int spot; "The spot number on the array."
int left; "The left coordinate of the spot on the scanned Microarray Image"
int top; "The top coordinate of the spot."
int right; "The right coordinate of the spot."
int bot; "The bottom coordinate of the spot."
string acc5; "The 5' GenBank accession number for an EST generated from the printed cDNA clone (This is the most stable handle for retrieving up to date information about this cDNA clone)"
string acc3; "The 3' GenBank accession number for an EST generated from the printed cDNA clone (This is the most stable handle for retrieving up to date information about this cDNA clone)"
)
table stsAlias
"STS marker aliases and associated identification numbers"
    (
    string alias;       "STS marker name"
    uint identNo;       "Identification number of STS marker"
    string trueName;        "Official UCSC name for marker"
    )
table stsInfo
"Constant STS marker information"
    (
    uint identNo;                 "UCSC identification number"
    string name;                  "Official UCSC name"
    uint gbCount;                 "Number of related GenBank accessions"
    string[gbCount] genbank;      "Related GeneBank accessions"
    uint gdbCount;                "Number of related GDB identifiers"
    string[gdbCount] gdb;         "Related GDB identifies"
    uint nameCount;               "Number of alias names"
    string[nameCount] otherNames; "Alias names"
    uint dbSTSid;                 "ID number in UniSTS or dbSTS"
    uint otherDbstsCount;         "Number of related dbSTS IDs"
    uint[otherDbstsCount] otherDbSTS;  "Related dbSTS IDs"

    string leftPrimer;            "5' primer sequence"
    string rightPrimer;           "3' primer sequence"
    string distance;              "Length of STS sequence"
    string organism;              "Organism for which STS discovered"
    uint sequence;                "Whether the full sequence is available (1) or not (0) for STS"

    uint otherUCSCcount;          "Number of related active UCSC ids"
    uint[otherUCSCcount] otherUCSC;  "Related active UCSC ids"
    uint mergeUCSCcount;          "Number of merged inactive UCSC ids"
    uint[mergeUCSCcount] mergeUCSC;  "Related merged inactive UCSC ids"

    string genethonName;          "Name in Genethon map"
    string genethonChr;           "Chromosome in Genethon map"
    float genethonPos;            "Position in Genethon map"
    float genethonLOD;            "LOD score in Genethon map"
    string marshfieldName;        "Name in Marshfield map"
    string marshfieldChr;         "Chromosome in Marshfield map"
    float marshfieldPos;          "Position in Marshfield map"
    float marshfieldLOD;          "LOD score in Marshfield map"
    string wiyacName;             "Name in WI YAC map"
    string wiyacChr;              "Chromosome in WI YAC map"
    float wiyacPos;               "Position in WI YAC map"
    float wiyacLOD;               "LOD score in WI YAC map"
    string wirhName;              "Name in WI RH map"
    string wirhChr;               "Chromosome in WI RH map"
    float wirhPos;                "Position in WI RH map"
    float wirhLOD;                "LOD score in WI RH map"
    string gm99gb4Name;           "Name in GeneMap99 GB4 map"
    string gm99gb4Chr;            "Chromosome in GeneMap99 GB4 map"
    float gm99gb4Pos;             "Position in GeneMap99 GB4 map"
    float gm99gb4LOD;             "LOD score in GeneMap99 GB4 map"
    string gm99g3Name;            "Name in GeneMap99 G3 map"
    string gm99g3Chr;             "Chromosome in GeneMap99 G3 map"
    float gm99g3Pos;              "Position in GeneMap99 G3 map"
    float gm99g3LOD;              "LOD score in GenMap99 G3 map"
    string tngName;               "Name in Stanford TNG map"
    string tngChr;                "Chromosome in Stanford TNG map"
    float tngPos;                 "Position in Stanford TNG map"
    float tngLOD;                 "LOD score in Stanford TNG map"
    )
table stsInfo2
"Constant STS marker information - revision"
    (
    uint identNo;                 "UCSC identification number"
    string name;                  "Official UCSC name"
    uint gbCount;                 "Number of related GenBank accessions"
    string[gbCount] genbank;      "Related GeneBank accessions"
    uint gdbCount;                "Number of related GDB identifiers"
    string[gdbCount] gdb;         "Related GDB identifies"
    uint nameCount;               "Number of alias names"
    string[nameCount] otherNames; "Alias names"
    uint dbSTSid;                 "ID number in UniSTS or dbSTS"
    uint otherDbstsCount;         "Number of related dbSTS IDs"
    uint[otherDbstsCount] otherDbSTS;  "Related dbSTS IDs"

    string leftPrimer;            "5' primer sequence"
    string rightPrimer;           "3' primer sequence"
    string distance;              "Length of STS sequence"
    string organism;              "Organism for which STS discovered"
    uint sequence;                "Whether the full sequence is available (1) or not (0) for STS"

    uint otherUCSCcount;          "Number of related active UCSC ids"
    uint[otherUCSCcount] otherUCSC;  "Related active UCSC ids"
    uint mergeUCSCcount;          "Number of merged inactive UCSC ids"
    uint[mergeUCSCcount] mergeUCSC;  "Related merged inactive UCSC ids"

    string genethonName;          "Name in Genethon map"
    string genethonChr;           "Chromosome in Genethon map"
    float genethonPos;            "Position in Genethon map"
    float genethonLOD;            "LOD score in Genethon map"
    string marshfieldName;        "Name in Marshfield map"
    string marshfieldChr;         "Chromosome in Marshfield map"
    float marshfieldPos;          "Position in Marshfield map"
    float marshfieldLOD;          "LOD score in Marshfield map"
    string wiyacName;             "Name in WI YAC map"
    string wiyacChr;              "Chromosome in WI YAC map"
    float wiyacPos;               "Position in WI YAC map"
    float wiyacLOD;               "LOD score in WI YAC map"
    string wirhName;              "Name in WI RH map"
    string wirhChr;               "Chromosome in WI RH map"
    float wirhPos;                "Position in WI RH map"
    float wirhLOD;                "LOD score in WI RH map"
    string gm99gb4Name;           "Name in GeneMap99 GB4 map"
    string gm99gb4Chr;            "Chromosome in GeneMap99 GB4 map"
    float gm99gb4Pos;             "Position in GeneMap99 GB4 map"
    float gm99gb4LOD;             "LOD score in GeneMap99 GB4 map"
    string gm99g3Name;            "Name in GeneMap99 G3 map"
    string gm99g3Chr;             "Chromosome in GeneMap99 G3 map"
    float gm99g3Pos;              "Position in GeneMap99 G3 map"
    float gm99g3LOD;              "LOD score in GenMap99 G3 map"
    string tngName;               "Name in Stanford TNG map"
    string tngChr;                "Chromosome in Stanford TNG map"
    float tngPos;                 "Position in Stanford TNG map"
    float tngLOD;                 "LOD score in Stanford TNG map"
    string decodeName;            "Name in deCODE map"
    string decodeChr;             "Chromosome in deCODE TNG map"
    float decodePos;              "Position in deCODE TNG map"
    float decodeLOD;              "LOD score in deCODE TNG map"
    )
table stsInfoMouse
"Constant STS marker information"
    (
    uint identNo;                 "UCSC identification number"
    string name;                  "Official UCSC name"
   
         
    uint MGIPrimerID;		   "sts Primer's MGI ID or 0 if N/A"
    string primerName;		   "sts Primer's name"
    string primerSymbol;	   "sts Primer's symbol"
    string primer1;          	   "primer1 sequence"
    string primer2;          	   "primer2 sequence"
    string distance;               "Length of STS sequence"
    uint sequence;                 "Whether the full sequence is available (1) or not (0) for STS"
    
   
    uint MGIMarkerID;		    "sts Marker's MGI ID or 0 if N/A"
    string stsMarkerSymbol;           "symbol of  STS marker"
    string Chr;                     "Chromosome in Genetic map"
    float geneticPos;               "Position in Genetic map. -2 if N/A, -1 if syntenic "
    string stsMarkerName;	    "Name of sts Marker."

    uint LocusLinkID;		    "Locuslink Id, 0 if N/A"
    )
table stsInfoMouseNew
"Constant STS marker information"
    (
    uint identNo;                 "UCSC identification number"
    string name;                  "Official UCSC name"
    uint MGIId;		   	  "Marker's MGI Id"
    string MGIName;		  "Marker's MGI name"	
    uint UiStsId;		  "Marker's UiStsId"	   
    uint nameCount;		  "Number of alias"
    string alias;		  "alias, or N/A"

    
    string primer1;          	   "primer1 sequence"
    string primer2;          	   "primer2 sequence"
    string distance;               "Length of STS sequence"
    uint sequence;                 "Whether the full sequence is available (1) or not (0) for STS"
    string organis; 		    "Organism for which STS discovered"
   
    string wigName;	    "WI_Mouse_Genetic map"
    string wigChr;                     "Chromosome in Genetic map"
    float wigGeneticPos;               "Position in Genetic map"
    
    string mgiName;	    "MGI map"
    string mgiChr;                     "Chromosome in Genetic map"
    float mgiGeneticPos;               "Position in Genetic map"

    string rhName;	    	    "WhiteHead_RH map"
    string rhChr;                     "Chromosome in Genetic map"
    float rhGeneticPos;               "Position in Genetic map."
    float RHLOD;		    "LOD score of RH map"


    string GeneName;               "Associated gene name"
    string GeneID;                 "Associated gene Id"
    string clone;		    "Clone sequence"
	)
table stsInfoRat
"Constant STS marker information"
    (
    uint identNo;                 "UCSC identification number"
    string name;                  "Official UCSC name"
    uint RGDId;		   	  "Marker's RGD Id"
    string RGDName;		  "Marker's RGD name"	
    uint UiStsId;		  "Marker's UiStsId"	   
    uint nameCount;		  "Number of alias"
    string alias;		  "alias, or N/A"

    
    string primer1;          	   "primer1 sequence"
    string primer2;          	   "primer2 sequence"
    string distance;               "Length of STS sequence"
    uint sequence;                 "Whether the full sequence is available (1) or not (0) for STS"
    string organis; 		    "Organism for which STS discovered"
   
    string fhhName;	    "Name in FHH_x_ACI map"
    string fhhChr;                     "Chromosome in Genetic map"
    float fhhGeneticPos;               "Position in Genetic map"
    
    string shrspName;	    "Name in SHRSP_x_BN"
    string shrspChr;                     "Chromosome in Genetic map"
    float shrspGeneticPos;               "Position in Genetic map"

    string rhName;	    	    "Name in RH map"
    string rhChr;                     "Chromosome in Genetic map"
    float rhGeneticPos;               "Position in Genetic map."
    float RHLOD;		    "LOD score of RH map"


    string GeneName;               "Associated gene name"
    string GeneID;                 "Associated gene Id"
    string clone;		    "Clone sequence"
	)
table stsMap
"STS marker and its position on golden path and various maps"
    (
    string chrom;	"Chromosome or 'unknown'"
    int chromStart;     "Start position in chrom - negative 1 if unpositioned"
    uint chromEnd;	"End position in chrom"
    string name;	"Name of STS marker"
    uint score;	        "Score of a marker = 1000/(# of placements)"

    uint identNo;	"Identification number of STS"
    string ctgAcc;	"Contig accession number"
    string otherAcc;	"Accession number of other contigs that the marker hits"

    string genethonChrom;	"Chromosome (no chr) from Genethon map or 0 if none"
    float genethonPos;		"Position on Genethon map"
    string marshfieldChrom;	"Chromosome (no chr) from Marshfield map or 0 if none"
    float marshfieldPos;	"Position on Marshfield map"
    string gm99Gb4Chrom;	"Chromosome (no chr) from GeneMap99 map or 0 if none"
    float gm99Gb4Pos;		"Position on gm99_bg4 map"
    string shgcTngChrom;	"Chromosome (no chr) from shgc_tng map or 0 if none"
    float shgcTngPos;		"Position on shgc_tng map"
    string shgcG3Chrom;		"Chromosome (no chr) from Stanford G3 map or 0 if none"
    float shgcG3Pos;		"Position on shgc_g3 map"
    string wiYacChrom;		"Chromosome (no chr) from Whitehead YAC map or 0 if none"
    float wiYacPos;		"Position on wi_yac map"
    string wiRhChrom;		"Chromosome (no chr) from Whitehead RH map or 0 if none"
    float wiRhPos;		"Position on wi_rh map"
    string fishChrom;		"Chromosome (no chr) from FISH map or 0 if none"
    string beginBand;		"Beginning of range of bands on FISH map"
    string endBand;		"End of range of bands on FISH map"
    string lab;			"Laboratory that placed the FISH clone"
    string decodeChrom;		"Chromosome (no chr) from deCODE genetic map or 0 if none"
    float decodePos;		"Position on deCODE genetic map"
    )
table stsMapMouse
"STS marker and its position on mouse assembly"
    (
    string chrom;	"Chromosome or 'unknown'"
    int chromStart;     "Start position in chrom - negative 1 if unpositioned"
    uint chromEnd;	"End position in chrom"
    string name;	"Name of STS marker"
    uint score;	        "Score of a marker = 1000/(# of placements)"
    uint identNo;	"UCSC Id No."
    uint probeId;       "Probe Identification number of STS"
    uint markerId; 	"Marker Identification number of STS"
    )
table stsMapMouseNew
"STS marker and its position on golden path and various maps"
    (
    string chrom;       "Chromosome or 'unknown'"
    int chromStart;     "Start position in chrom - negative 1 if unpositioned"
    uint chromEnd;      "End position in chrom"
    string name;        "Name of STS marker"
    uint score;         "Score of a marker = 1000/(# of placements)"

    uint identNo;       "Identification number of STS"
    string ctgAcc;      "Contig accession number"
    string otherAcc;    "Accession number of other contigs that the marker hits"

    string rhChrom;       "Chromosome (no chr) from RH map or 0 if none"
    float rhPos;          "Position on rh map"
    float rhLod;	  "Lod score of RH map"
    string wigChr;     	  "Chromosome (no chr) from FHHxACI genetic or 0 if none"
    float wigPos;        "Position on FHHxACI map"
    string mgiChrom;        "Chromosome (no chr) from SHRSPxBN geneticmap or 0 if none"
    float mgiPos;           "Position on SHRSPxBN genetic map"
    )
table stsMapRat
"STS marker and its position on golden path and various maps"
    (
    string chrom;       "Chromosome or 'unknown'"
    int chromStart;     "Start position in chrom - negative 1 if unpositioned"
    uint chromEnd;      "End position in chrom"
    string name;        "Name of STS marker"
    uint score;         "Score of a marker = 1000/(# of placements)"

    uint identNo;       "Identification number of STS"
    string ctgAcc;      "Contig accession number"
    string otherAcc;    "Accession number of other contigs that the marker hits"

    string rhChrom;       "Chromosome (no chr) from RH map or 0 if none"
    float rhPos;          "Position on rh map"
    float rhLod;	  "Lod score of RH map"
    string fhhChr;     	  "Chromosome (no chr) from FHHxACI genetic or 0 if none"
    float fhhPos;        "Position on FHHxACI map"
    string shrspChrom;        "Chromosome (no chr) from SHRSPxBN geneticmap or 0 if none"
    float shrspPos;           "Position on SHRSPxBN genetic map"
    )
table stsMarker
"STS marker and its position on golden path and various maps"
    (
    string chrom;	"Chromosome or 'unknown'"
    int chromStart; "Start position in chrom - negative 1 if unpositioned"
    uint chromEnd;	"End position in chrom"
    string name;	"Name of STS marker"
    uint score;	"Score of a marker = 1000/# of markers it hits"

    uint identNo;	"Identification number of STS"
    string ctgAcc;	"Contig accession number"
    string otherAcc;	"Accession number of other contigs that the marker hits"

    string genethonChrom;	"Chromosome (no chr) from Genethon map or 0 if none"
    float genethonPos;		"Position on Genethon map"
    string marshfieldChrom;	"Chromosome (no chr) from Marshfield map or 0 if none"
    float marshfieldPos;	"Position on Marshfield map"
    string gm99Gb4Chrom;	"Chromosome (no chr) from gm99_bg4 map or 0 if none"
    float gm99Gb4Pos;		"Position on gm99_bg4 map"
    string shgcG3Chrom;		"Chromosome (no chr) from shgc_g3 map or 0 if none"
    float shgcG3Pos;		"Position on shgc_g3 map"
    string wiYacChrom;		"Chromosome (no chr) from wi_yac map or 0 if none"
    float wiYacPos;		"Position on wi_yac map"
    string shgcTngChrom;	"Chromosome (no chr) from shgc_tng map or 0 if none"
    float shgcTngPos;		"Position on shgc_tng map"
    string fishChrom;		"Chromosome (no chr) from FISH map or 0 if none"
    string beginBand;		"Beginning of range of bands on FISH map"
    string endBand;		"End of range of bands on FISH map"
    string lab;			"Laboratory that placed the FISH clone"
    )

table synMap
"Synteny map from one genome to another"
(
string chromA;  "chromosome in organism A"
uint chromStartA; "chromosome start in orgainsm A"
uint chromEndA;   "chromosome end in orgainsm A"
string strandA;  "Strand information can be things like '-+'"
string chromB;  "chromosome in organism B"
uint chromStartB; "chromosome start in orgainsm B"
uint chromEndB;   "chromosome end in orgainsm B"
)
table synteny100000
"Mouse Synteny from blastz single coverage"
(
string chrom;       "Human Chrom"
uint chromStart;       "Start on Human"
uint chromEnd;         "End on Human"
string mouseChrom;   "Mouse Chromosome"
uint score;     "score always zero"
char[1] strand; "+ direction matches - opposite"
)

table syntenyBerk
"UC Berkeley Mouse Synteny"
(
string chrom;       "Human Chrom"
uint chromStart;       "Start on Human"
uint chromEnd;         "End on Human"
string name;   "Mouse Chromosome"
uint score;     "score always zero"
char[1] strand; "+ direction matches - opposite"
)
table syntenySanger
"Sanger Mouse Synteny"
(
string chrom;       "Human Chrom"
uint chromStart;       "Start on Human"
uint chromEnd;         "End on Human"
string name;   "Mouse Chromosome"
uint score;     "score always zero"
char[1] strand; "+ direction matches - opposite"
)
table tableDescriptions
"Descriptive information about database tables from autoSql / gbdDescriptions"
    (
    string  tableName;	"Name of table (with chr*_ replaced with chrN_)"
    lstring autoSqlDef;	"Contents of autoSql (.as) table definition file"
    string  gbdAnchor;	"Anchor for table description in gbdDescriptions.html"
    )
table tfbsCons
"tfbsCons Data"
    (
    string chrom;      "Human chromosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    char[6] species;  "common name, scientific name"
    char[64] factor;  "factor "
    char[10] id;  "id"
    )
table tfbsConsMap
"tfbsConsMap Data"
    (
    string id;   "TRANSFAC id"
    string ac;   "gene-regulation.com AC"
    )
table tigrCmrGene
"For TIGR CMR genes tracks"
    (
    short  bin;            "Bin number for browser speedup"
    string chrom;          "Human chromosome or FPC contig"
    uint chromStart;       "Start position in chromosome"
    uint chromEnd;         "End position in chromosome"
    string name;           "TIGR locus"
    uint score;            "Score from 900-1000.  1000 is best"
    char[1] strand;        "Value should be + or -"
    lstring tigrCommon;    "TIGR Common Name"
    string tigrGene;       "TIGR Gene Symbol"
    string tigrECN;        "TIGR Enzyme Commission Number"
    string primLocus;      "Primary Locus Name"
    uint tigrLength;       "TIGR sequence length"
    uint tigrPepLength;    "TIGR Protein length"
    lstring tigrMainRole;  "TIGR Main Role"
    lstring tigrSubRole;   "TIGR Sub Role"
    string swissProt;      "SwissProt TrEMBL Accession"
    string genbank;        "Genbank ID"
    float tigrMw;          "Molecular Weight"
    float tigrPi;          "Isoelectric point (I think)"
    float tigrGc;          "GC content"
    string goTerm;         "GO term (gene ontology)"
    )
table tilingPath
"A tiling path of clones through a chromosome"
(
string chrom;	"Chromosome name: chr1, chr2, etc."
string accession; "Clone accession or ? or GAP"
string clone;     "Clone name in BAC library"
string contig;    "Contig (or gap size)"
int chromIx;      "Number of clone in tiling path starting chrom start"
)
table traceInfo
"subset of the sequencing trace ancillary information, see http://www.ncbi.nlm.nih.gov/Traces/TraceArchiveRFC.html"
    (
    string ti;		"Trace identifier"
    string templateId;	"Name of the template"
    uint size;		"Size of read"
    )

table trackDb
"This describes an annotation track."
(
string tableName; "Symbolic ID of Track"
string shortLabel; "Short label displayed on left"
string type;      "Track type: bed, psl, genePred, etc."
string longLabel; "Long label displayed in middle"
ubyte visibility; "0=hide, 1=dense, 2=full, 3=pack"
float priority;   "0-100 - where to position.  0 is top"
ubyte colorR;	  "Color red component 0-255"
ubyte colorG;	  "Color green component 0-255"
ubyte colorB;	  "Color blue component 0-255"
ubyte altColorR;  "Light color red component 0-255"
ubyte altColorG;  "Light color green component 0-255"
ubyte altColorB;  "Light color blue component 0-255"
ubyte useScore;   "1 if use score, 0 if not"
ubyte private;	  "1 if only want to show it on test site"
int restrictCount;	"Number of chromosomes this is on (0=all though!)"
string[restrictCount] restrictList; "List of chromosomes this is on"
lstring url;	"URL to link to when they click on an item"
lstring html;   "Some html to display when they click on an item"
string grp;   "Which group track belongs to"
ubyte canPack; "1 if can pack track display, 0 otherwise"
lstring settings; "Name/value pairs for track-specific stuff"
)
table trackTable
"This describes an annotation track."
(
string mapName; "Symbolic ID of Track"
string tableName; "Name of table (without any chrN_ if split)"
string shortLabel; "Short label displayed on left"
string longLabel; "Long label displayed in middle"
ubyte visibility; "0=hide, 1=dense, 2=full"
ubyte colorR;	  "Color red component 0-255"
ubyte colorG;	  "Color green component 0-255"
ubyte colorB;	  "Color blue component 0-255"
ubyte altColorR;  "Light color red component 0-255"
ubyte altColorG;  "Light color green component 0-255"
ubyte altColorB;  "Light color blue component 0-255"
ubyte useScore;   "1 if use score, 0 if not"
ubyte isSplit;    "1 if table is split across chromosomes"
ubyte private;	  "1 if only want to show it on test site"
ubyte hardCoded;  "1 if code to interpret it is build in"
)
table vegaInfo
"Vega Genes track additional information"
    (
    string transcriptId;	"Vega transcript ID"
    string otterId;		"otter (Ensembl db) transcript ID"
    string geneId;		"Vega gene ID"
    string method;		"GTF method field"
    string geneDesc;		"Vega gene description"
    )
table wabAli
"Information on a waba alignment" 
    (
    string query;	"Foreign sequence name."
    uint qStart;        "Start in query (other species)"
    uint qEnd;          "End in query."
    char[1] strand;     "+ or - relative orientation."
    string chrom;       "Chromosome (current species)."
    uint chromStart;    "Start in chromosome."
    uint chromEnd;      "End in chromosome."
    uint milliScore;    "Base identity in parts per thousand."
    uint symCount;      "Number of symbols in alignment."
    lstring qSym;       "Query bases plus '-'s."
    lstring tSym;       "Target bases plus '-'s."
    lstring hSym;       "Hidden Markov symbols."
    )

table wiggle
"Wiggle track values to display as y-values (first 6 fields are bed6)"
(
string chrom;         "Human chromosome or FPC contig"
uint chromStart;      "Start position in chromosome"
uint chromEnd;        "End position in chromosome"
string name;          "Name of item"
uint span;            "each value spans this many bases"
uint count;           "number of values in this block"
uint offset;          "offset in File to fetch data"
string file;          "path name to data file, one byte per value"
double lowerLimit;    "lowest data value in this block"
double dataRange;     "lowerLimit + dataRange = upperLimit"
uint validCount;      "number of valid data values in this block"
double sumData;       "sum of the data points, for average and stddev calc"
double sumSquares;    "sum of data points squared, for stddev calc"
)
table displayId
"Relate ID and primary accession. A good table to use just get handle on all records."
    (
    char[8] acc;	"Primary accession"
    char[12] val;	"SwissProt display ID"
    )

table otherAcc
"Relate ID and other accessions"
    (
    char[8] acc;	"Primary accession"
    char[8] val;	"Secondary accession"
    )

table organelle
"A part of a cell that has it's own genome"
    (
    int id;	"Organelle ID - we create this"
    string val;	"Text description"
    )

table info
"Small stuff with at most one copy associated with each SwissProt record"
    (
    char[8] acc;	"Primary accession"
    byte isCurated;	"True if curated (SwissProt rather than trEMBL)"
    int aaSize;		"Size in amino acids"
    int molWeight;	"Molecular weight"
    string createDate;	"Creation date"
    string seqDate;	"Sequence last update date"
    string annDate;	"Annotation last update date"
    int organelle;	"Pointer into organelle table"
    )

table description
"Description lines"
    (
    char[8] acc;	"Primary accession"
    lstring val; 	"SwissProt DE lines"
    )

table geneLogic
"Gene including and/or logic if multiple"
    (
    char[8] acc;	"Primary accession"
    lstring val;	"Gene(s) and logic to relate them."
    )

table gene
"Gene/accession relationship. Both sides can be multiply valued."
    (
    char[8] acc;	"Primary accession"
    string val;		"Single gene name"
    )

table taxon
"An NCBI taxon"
    (
    int id;		"Taxon NCBI ID"
    string binomial;	"Binomial format name"
    lstring toGenus;	"Taxonomy - superkingdom to genus"
    )

table commonName
"Common name for a taxon"
    (
    int taxon;	"Taxon table ID"
    string val; "Common name"
    )

table accToTaxon
"accession/taxon relationship"
    (
    char[8] acc;	"Primary accession"
    int	taxon;		"ID in taxon table"
    )

table keyword
"A keyword"
    (
    int id;	"Keyword ID - we create this"
    string val;	"Keyword itself"
    )

table accToKeyword
"Relate keywords and accessions"
    (
    char[8] acc;	"Primary accession"
    int keyword;	"ID in keyword table"
    )

table commentType
"A type of comment"
    (
    int id;	"Comment type ID, we create this"
    string val;	"Name of comment type"
    )

table commentVal
"Text of a comment"
    (
    int id;	"Comment value ID - we create this"
    lstring val;	"Text of comment."
    )

table comment
"A structured comment"
    (
    char[8] acc;     "Primary accession"
    int commentType; "ID in commentType table"
    int commentVal;  "ID in commentVal table"
    )

table protein
"Amino acid sequence"
    (
    char[8] acc;	"Primary accession"
    lstring val;	"Amino acids"
    )

table extDb
"Name of another database"
    (
    int id;	"Database id - we make this up"
    string val;	"Name of database"
    )

table extDbRef
"A reference to another database"
    (
    char[8] acc;	"Primary SwissProt accession"
    int extDb;		"ID in extDb table"
    string extAcc1;	"External accession"
    string extAcc2;	"External accession"
    string extAcc3;	"External accession"
    )

table featureClass
"A class of feature"
    (
    int id;	"Database id - we make this up"
    string val;	"Name of class"
    )

table featureType
"A type of feature"
    (
    int id;	"Database id - we make this up"
    lstring val;	"Name of type"
    )

table feature
"A description of part of a protein"
    (
    char[8] acc;	"Primary accession"
    int start;	"Start coordinate (zero based)"
    int end;	"End coordinate (non-inclusive)"
    int featureClass;	"ID of featureClass"
    int featureType;    "ID of featureType"
    byte softEndBits;  "1 for start <, 2 for start ?, 4 for end >, 8 for end ?"
    )

table author
"A single author"
    (
    int id;	"ID of this author"
    string val;	"Name of author"
    )

table reference
"An article (or book or patent) in literature."
    (
    int id;	"ID of this reference"
    lstring title; "Title"
    lstring cite; "Enough info to find journal/patent/etc."
    string pubMed; "Pubmed cross-reference"
    string medline; "Medline cross-reference"
    )

table referenceAuthors
"This associates references and authors"
    (
    int reference;	"ID in reference table"
    int author;		"ID in author table"
    )

table citationRp
"SwissProt RP (Reference Position) line.  Often includes reason for citing."
    (
    int id;	"ID of this citationRp"
    lstring val;	"Reason for citing/position in sequence of cite."
    )

table citation
"A SwissProt citation of a reference"
    (
    int id;		"ID of this citation"
    char[8] acc;	"Primary accession"
    int reference;	"ID in reference table"
    int rp;		"ID in rp table"
    )

table rcType
"Types found in a swissProt reference RC (reference comment) line"
    (
    int id;	"ID of this one"
    string val; "name of this"
    )

table rcVal
"Values found in a swissProt reference RC (reference comment) line"
    (
    int id;	"ID of this"
    lstring val; "associated text"
    )

table citationRc
"Reference comments associated with citation"
    (
    int citation;	"ID in citation table"
    int rcType;		"ID in rcType table"
    int rcVal;		"ID in rcVal table"
    )

table psl
"Summary info about a patSpace alignment"
    (
    uint matches;  "Number of bases that match that aren't repeats"
    uint misMatches; "Number of bases that don't match"
    uint repMatches; "Number of bases that match but are part of repeats"
    uint nCount;  "Number of 'N' bases"
    uint qNumInsert; "Number of inserts in query"
    int qBaseInsert; "Number of bases inserted in query"
    uint tNumInsert; "Number of inserts in target"
    int tBaseInsert; "Number of bases inserted in target"
    char[2] strand; "+ or - for strand. First character query, second target (optional)"
    string qName; "Query sequence name"
    uint qSize; "Query sequence size"
    uint qStart; "Alignment start position in query"
    uint qEnd; "Alignment end position in query"
    string tName; "Target sequence name"
    uint tSize; "Target sequence size"
    uint tStart; "Alignment start position in target"
    uint tEnd; "Alignment end position in target"
    uint blockCount; "Number of blocks in alignment"
    uint[blockCount] blockSizes; "Size of each block"
    uint[blockCount] qStarts; "Start of each block in query."
    uint[blockCount] tStarts; "Start of each block in target."
    )
