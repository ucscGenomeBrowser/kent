table udHistory
"Records alterations made to ultra database"
    (
    string who;	"Name of user who made change"
    string program;	"Name of program that made change"
    string time;	"When alteration was made"
    lstring changes;	"CGI-ENCODED string describing changes"
    )
