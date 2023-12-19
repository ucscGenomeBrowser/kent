#Lou - 14/08/19

#Make a list of all assemblies in hgCentral with active=1
allDbs = get_ipython().getoutput(u'hgsql -h genome-centdb -e "SELECT name FROM dbDb WHERE active = 1 ORDER BY RAND()" hgcentral | grep -v "name"')

curatedHubs=['hs1','mpxvRivers']

#Initialize list to hold assemblies with errors
troubleDbs = []

#Iterate through allDbs and run checkSync.csh comparing to hgw1
for db in allDbs:
    checkSyncResults = get_ipython().getoutput(u"checkSync.csh '$db' hgw1 hgwbeta")
    if db in curatedHubs:
        continue
    if db == 'mm9':
        if '  0 hgw1.only' not in str(checkSyncResults) and '  4 hgwbeta.only' not in str(checkSyncResults):
            troubleDbs.append(db)
    elif db == 'mm39':
        if '  2 hgw1.only' not in str(checkSyncResults) and '  2 hgwbeta.only' not in str(checkSyncResults):
            troubleDbs.append(db)
    elif db == 'hg18':
        if '  0 hgw1.only' not in str(checkSyncResults) and '  4 hgwbeta.only' not in str(checkSyncResults):
            troubleDbs.append(db)
    elif db == 'hg19':
        if '  0 hgw1.only' not in str(checkSyncResults) and '  5 hgwbeta.only' not in str(checkSyncResults):
            troubleDbs.append(db)
    elif db == 'hg38':
        if '  3 hgw1.only' not in str(checkSyncResults) and '  3 hgwbeta.only' not in str(checkSyncResults):
            troubleDbs.append(db)
    else:
        if '  0 hgw1.only' not in str(checkSyncResults) and '  2 hgwbeta.only' not in str(checkSyncResults):
            troubleDbs.append(db)
    
#Iterate through allDbs and run checkSync.csh comparing to hgw2, informing of discrepancies
for db in allDbs:
    checkSyncResults = get_ipython().getoutput(u"checkSync.csh '$db' hgw2 hgwbeta")
    if db in curatedHubs:
        continue
    if db == 'mm9':
        if '  0 hgw1.only' not in str(checkSyncResults) and '  4 hgwbeta.only' not in str(checkSyncResults)             and db not in troubleDbs:
            get_ipython().system(u" echo There looks to be a discrepancy between hgw1 and hgw2 checkSync for: '$db'")
            get_ipython().system(u" echo Follow up with checkSync.csh '$db' hgw1 hgw2")
            get_ipython().system(u' echo')
            troubleDbs.append(db)
    elif db == 'hg18':
        if '  0 hgw1.only' not in str(checkSyncResults) and '  4 hgwbeta.only' not in str(checkSyncResults)             and db not in troubleDbs:
            get_ipython().system(u" echo There looks to be a discrepancy between hgw1 and hgw2 checkSync for: '$db'")
            get_ipython().system(u" echo Follow up with checkSync.csh '$db' hgw1 hgw2")
            get_ipython().system(u' echo')
            troubleDbs.append(db)
    elif db == 'hg19':
        if '  0 hgw1.only' not in str(checkSyncResults) and '  5 hgwbeta.only' not in str(checkSyncResults)             and db not in troubleDbs:
            get_ipython().system(u" echo There looks to be a discrepancy between hgw1 and hgw2 checkSync for: '$db'")
            get_ipython().system(u" echo Follow up with checkSync.csh '$db' hgw1 hgw2")
            get_ipython().system(u' echo')
            troubleDbs.append(db)
    elif db == 'hg38':
        if '  0 hgw1.only' not in str(checkSyncResults) and '  3 hgwbeta.only' not in str(checkSyncResults)             and db not in troubleDbs:
            get_ipython().system(u" echo There looks to be a discrepancy between hgw1 and hgw2 checkSync for: '$db'")
            get_ipython().system(u" echo Follow up with checkSync.csh '$db' hgw1 hgw2")
            get_ipython().system(u' echo')
            troubleDbs.append(db)
    else:
        if '  0 hgw1.only' not in str(checkSyncResults) and '  2 hgwbeta.only' not in str(checkSyncResults)             and db not in troubleDbs:
            get_ipython().system(u" echo There looks to be a discrepancy between hgw1 and hgw2 checkSync for: '$db'")
            get_ipython().system(u" echo Follow up with checkSync.csh '$db' hgw1 hgw2")
            get_ipython().system(u' echo')
            troubleDbs.append(db)
    
#Only if an error is found, rerun checkSyncs and display those results
if troubleDbs == []:
    get_ipython().system(u' echo "There were no errors found between hgw1/hgw2 and beta tables (checkSync.csh)"')
    get_ipython().system(u' echo')
else:
    get_ipython().system(u' echo Discrepancies between hgw1/hgw2 and beta tables:')
    get_ipython().system(u' echo')
    for db in troubleDbs:
        get_ipython().system(u" echo checkSync.csh for '$db'")
        get_ipython().system(u" checkSync.csh '$db' hgw1 hgwbeta")

troubleDbs = []

#Run checkMetaData on allDbs filtering out for only unexpected results
for db in allDbs:
    checkMetaDataResults = get_ipython().getoutput(u"checkMetaData.csh '$db' rr hgwbeta")
    if str('0 dbDb.'+db+'.hgcentralOnly') and str('0 dbDb.'+db+'.hgcentralbetaOnly')     and str('0 blatServers.'+db+'.hgcentralOnly') and str('0 blatServers.'+db+'.hgcentralbetaOnly')     and str('0 defaultDb.'+db+'.hgcentralOnly') and str('0 defaultDb.'+db+'.hgcentralbetaOnly')     and str('0 genomeClade.'+db+'.hgcentralOnly') and str('0 genomeClade.'+db+'.hgcentralbetaOnly')     and str('0 liftOverChain.'+db+'.hgcentralOnly') and str('0 liftOverChain.'+db+'.hgcentralbetaOnly')     not in str(checkMetaDataResults):
        troubleDbs.append(db)
        
#Only if an error is found, rerun checkMetaData and display those results
if troubleDbs == []:
    get_ipython().system(u' echo "There were no errors found between rr and hgwbeta MetaData (checkMetaData.csh)"')
    get_ipython().system(u' echo')
else:
    get_ipython().system(u' echo Discrepancies between rr and hgwbeta MetaData:')
    get_ipython().system(u' echo')
    for db in troubleDbs:
        get_ipython().system(u" checkMetaData.csh '$db' rr hgwbeta")
        
get_ipython().system(u' echo')
get_ipython().system(u' echo Additional Resources:')
get_ipython().system(u' echo checkMetaData example 1: http://genomewiki.ucsc.edu/genecats/index.php/Monitoring_Tasks_Notes#Cronjob:_Results_from_checkMetaAday.csh')
get_ipython().system(u' echo checkMetaData example 2: http://genomewiki.ucsc.edu/genecats/index.php/Assembly_QA_Part_4_RR_Steps#RR:_checkMetaData')
get_ipython().system(u' echo RM ticket: http://redmine.soe.ucsc.edu/issues/23998')
        
#Remove all temp files
get_ipython().system(u' rm blatServers.*')
get_ipython().system(u' rm dbDb.*')
get_ipython().system(u' rm defaultDb.*')
get_ipython().system(u' rm genomeClade.*')
get_ipython().system(u' rm liftOverChain.*')

