table snpExceptions
"Set of queries to look for snps that appear problematic"
    (
    uint    exceptionId; "unique ID for this exception"
    string  query;       "SQL string to retrieve bad records"
    uint    num;         "Count of SNPs that fail this condition"
    string  description; "Text string for readability"
    string  resultPath;  "path for results file"
    )
