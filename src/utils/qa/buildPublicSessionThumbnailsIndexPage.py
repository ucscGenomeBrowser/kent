#!/usr/bin/python3

import os, requests, subprocess, sys, argparse

def parseArgs():
    """
    Parse the command line arguments.
    """
    parser = argparse.ArgumentParser(description = __doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    optional = parser._action_groups.pop()

    required = parser.add_argument_group('required arguments')

    required.add_argument ("saveDir",
        help = "Server from which to query the hubPublic table. Can be dev, hgwbeta, or rr.")
    if (len(sys.argv) == 1):
        parser.print_usage()
        print("\nFetches thumbnails and descriptions of public sessions from the RR and saves/creates\n" + \
              "an html page that can be included in the homepage. Requires the directory in which to save\n" + \
              "the pages to be declared.\n\n" + \
              "Example run:\n" + \
              "    buildPublicSessionThumbnailsIndexPage /cluster/home/lrnassar/kent/src/hg/htdocs/\n")
        exit(0)
    parser._action_groups.append(optional)
    options = parser.parse_args()
    return  options

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

def queryHgPublicSessAndAssignOutputFileAndDir(saveDir):
    """Curl hgPublicSession and assign output file name/directory"""
    hgPublicSessionOutPut = bash('curl -Lk https://genome.ucsc.edu/cgi-bin/hgPublicSessions')
    outputFile = open(saveDir+'thumbNailLinks.html', 'w')
    return(hgPublicSessionOutPut, outputFile)

def parseHgPublicSessPageAndWriteOut(hgPublicSessionOutPut, saveDir, outputFile):
    """Parse hgPublicSession curled page, extract thumbnail/url/description and write out to file"""
    sessionThumbNailsWritten = 1
    for line in hgPublicSessionOutPut.split('\n'):
        if "trash" in line and sessionThumbNailsWritten < 4:
            sessionUrl = "https://genome.ucsc.edu/cgi-bin/"+line.split('"')[1].split("/")[2]
            trashDirUrl = line.split('"')[3]
            downloadThumbNailUrl = "https://genome.ucsc.edu/trash/hgPS/"+trashDirUrl.split("/")[3]
            currentImageFileName = 'sessionThumbNail'+str(sessionThumbNailsWritten)+'.png'
            img_data = requests.get(downloadThumbNailUrl).content
            with open(currentImageFileName, 'wb') as handler:
                handler.write(img_data)
            bash('mv '+currentImageFileName+' '+saveDir+currentImageFileName)
            bash('chmod 777 '+saveDir+currentImageFileName)
        if "Description:" in line and sessionThumbNailsWritten < 4:
            sessionThumbNailsWritten+=1
            sessionDescription = line.split('</b> ')[1].split('<br>')[0]
            outputFile.write('''<a href="'''+sessionUrl+'''">\n<img src="/'''+currentImageFileName+'''" title="'''
                         +sessionDescription+'''" class="sessionThumbnail" style="vertical-align: top;" width="30%" 
                         height=40%"></img></a>\n''')   
    outputFile.close()        
    bash('chmod 777 '+saveDir+'thumbNailLinks.html')
    
def main():
    options = parseArgs()
    saveDir = options.saveDir
    hgPublicSessionOutPut, outputFile = queryHgPublicSessAndAssignOutputFileAndDir(saveDir)
    parseHgPublicSessPageAndWriteOut(hgPublicSessionOutPut, saveDir, outputFile)
    
main()
