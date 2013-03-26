CREATE TABLE gadAll (
	id 		varchar(20),  #	ID
	association	char(1),      #	Association(Y/N)
	broadPhen	varchar(255), #	Broad Phenotype
	diseaseClass	varchar(20),  #	Disease Class
	diseaseClassCode varchar(20), # Disease Class
	meshTerm        blob,         # MeSH Disease Terms
	chromosome	varchar(20),  #	Chromosome
	band		varchar(20),  #	Chr-Band
	geneSymbol	varchar(40),  #	Gene
	chromStart	int unsigned, #	DNA Start
	chromEnd	int unsigned, # DNA End
	pValue		varchar(255), #	P Value
	reference 	varchar(255), #	Reference
	pubMed		varchar(40),  #	Pubmed ID
	alleleAuthDes	varchar(255), #	Allele Author Discription
	alleleFunEffect	varchar(255), #	Allele Functional Effects
	polymClass	varchar(40),  #	Polymophism Class
	geneName	varchar(255), #	Gene Name
	refSeq 		varchar(255), #	RefSeq ID
	population	varchar(255), #	Population
	meshGeoloc	varchar(255), # MeSH Geolocation
	submitter	varchar(255), #	Submitter
	locusNum 	varchar(40),  #	Locus Number
	unigene		varchar(20),  #	Unigene
	narrowPhen	varchar(255), #	Narrow Phenotype
	molePhenotype	varchar(255), #	Mole. Phenotype
	journal		varchar(255), #	Journal
	title		blob,         #	Title
	rsId		varchar(255), #	rs Number
	omimId		varchar(20),  #	OMIM ID
	gadQ		varchar(255), #	GAD/CDC (unpopulated as of 2013)
	gadCdc		varchar(255), #	GAD/CDC (unpopulated as of 2013)
	year		varchar(10),  #	Year
	conclusion	blob, 	      #	Conclusion
	studyInfo	blob,         #	Study Info
	envFactor	varchar(255), #	Env. Factor
	giGeneA		varchar(255), #	GI Gene A
	giAlleleGeneA	varchar(255), #	GI Allele of Gene A
	giGeneB		varchar(255), #	GI Gene B
	giAlleleGeneB	varchar(255), #	GI Allele of Gene B
	giGeneC		varchar(255), #	GI Gene C
	giAlleleGeneC	varchar(255), #	GI Allele of Gene C
	giAssociation	varchar(255), #	GI Association?
	giEnv		varchar(255), # GI combine Env. Factor
	giDis		varchar(255), # GI relevant to Disease
	KEY(id),
	KEY(geneSymbol)
);
	    
