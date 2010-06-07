CREATE TABLE immunoRaw (
subjId          char(40),	# GSID ID 
InjPriInf       char(40), 	# Injections Prior to Infection 
SDayLastPTest   char(40), 	# Study Day of Last Peak Test 
LastPVisit      char(40),	# Last Peak Visit (Month) 
LastPCD4Blk     char(40), 	# Last Peak CD4 Blocking 
LastPMNNeutral  char(40),	# Last Peak MN Neutralization 
LastPAntiGP120  char(40),	# Last Peak anti-gp120 
SDayLastTrTest  char(40),	# Study Day of Last Trough Test 
LastTrVisit     char(40),	# Last Trough Visit (Month) 
LastTrCD4Blk    char(40),	# Last Trough CD4 Blocking 
LastTrMnNeutral char(40), 	# Last Trough MN Neutralization 
LastTrAntiGP120 char(40),	# Last Trough anti-gp120 
KEY subjId(subjId)
) TYPE=MyISAM;
