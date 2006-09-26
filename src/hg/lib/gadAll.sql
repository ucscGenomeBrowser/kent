CREATE TABLE gadAll (
	id 		varchar(20),  #	ID
	association	char(1),      #	Association(Y/N)
	broadPhen	varchar(255), #	Broad Phenotype
	diseaseClass	varchar(20),  #	Disease Class
	diseaseClassCode varchar(20), # Disease Class
	chromosome	varchar(20),  #	Chromosome
	band		varchar(20),  #	Chr-Band
	geneSymbol	varchar(40),  #	Gene
	chromStart	int(10),      #	DNA Start
	chromEnd	int(10),      # DNA End
	pValue		varchar(20),  #	P Value
	reference 	varchar(255), #	Reference
	pubMed		varchar(40),  #	Pubmed ID
	alleleAuthDes	varchar(255), #	Allele Author Discription
	alleleFunEffect	varchar(255), #	Allele Functional Effects
	polymClass	varchar(40),  #	Polymophism Class
	geneName	varchar(255), #	Gene Name
	geneComment	varchar(255), #	Gene Comments
	population	varchar(255), #	Population
	comment		varchar(255), #	Comments
	submitter	varchar(255), #	Submitter
	locusNum 	varchar(40),  #	Locus Number
	unigene		varchar(20),  #	Unigene
	narrowPhen	varchar(255), #	Narrow Phenotype
	molePhenotype	varchar(255), #	Mole. Phenotype
	journal		varchar(255), #	Journal
	title		varchar(255), #	Title
	rsId		varchar(20),  #	rs Number
	omimId		varchar(20),  #	OMIM ID
	index4		varchar(40),  #	INDEX4
	gadCdc		varchar(255), #	GAD/CDC
	year		varchar(10),  #	Year
	conclusion	blob, 	      #	Conclusion
	studyInfo	varchar(255), #	Study Info
	envFactor	varchar(255), #	Env. Factor
	giGeneA		varchar(255), #	GI Gene A
	giAlleleGeneA	varchar(255), #	GI Allele of Gene A
	giGeneB		varchar(255), #	GI Gene B
	giAlleleGeneB	varchar(255), #	GI Allele of Gene B
	giGeneC		varchar(255), #	GI Gene C
	giAlleleGeneC	varchar(255), #	GI Allele of Gene C
	giAssociation	varchar(255), #	GI Association?
	giEnvFactor	varchar(255), #	GI combine Env. Factor
	giGIToDisease	varchar(255), #	GI relevant to Disease

	KEY(id)
);
	    
