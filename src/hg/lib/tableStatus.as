table tableStatus
"This describe the fields of returned by mysql's show table status"
    (
    string name;	"Name of table"
    string type;	"How table stored: MyISAM/INNOdb etc"
    string rowFormat;	"Seems to be mostly Fixed or Dynamic"
    uint rows;		"The number of rows in table"
    uint aveRowLength;	"Average length of row in bytes"
    bigint dataLength;	"Size of data in table"
    bigint maxDataLength; "Maximum data length for this table type"
    bigint indexLength;	"Length of index in bytes"
    bigint dataFree;	"Usually zero"
    string autoIncrement; "Current value of autoincrement - rows+1 or NULL"
    string createTime;  "YYYY-MM-DD HH:MM:SS format creation time"
    string updateTime;  "YYYY-MM-DD HH:MM:SS format last update time"
    string checkTime;   "YYYY-MM-DD HH:MM:SS format last check (not query) time"
    string createOptions; "Seems to be mostly blank"
    string comment;	"Seems mostly to be blank"
    )

