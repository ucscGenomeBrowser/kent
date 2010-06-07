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
