table blatServers
"Description of online BLAT servers"
    (
    string db;	"Database name"
    string host;	"Host (machine) name"
    int port;	"TCP/IP port on host"
    byte isTrans;  "0 for nucleotide/1 for translated nucleotide"
    byte canPcr;  "1 for use with PCR, 0 for not"
    byte dynamic;  "1 for if gfServer is dynamic under xinetd, 0 for not"
    )
