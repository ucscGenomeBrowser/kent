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
