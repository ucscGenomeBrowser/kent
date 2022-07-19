#!/usr/bin/python

import subprocess, os, sys
from PIL import Image, ImageChops
from datetime import date
import getpass

# Notes
# Currently this only supports a single session comparison
# But the logic is there to support any number if the
# Output and comparison functions are updated

### Functions

def run(*popenargs, **kwargs):
    input = kwargs.pop("input", None)
    check = kwargs.pop("handle", False)

    if input is not None:
        if 'stdin' in kwargs:
            raise ValueError('stdin and input arguments may not both be used.')
        kwargs['stdin'] = subprocess.PIPE

    process = subprocess.Popen(*popenargs, **kwargs)
    try:
        stdout, stderr = process.communicate(input)
    except:
        process.kill()
        process.wait()
        raise
    retcode = process.poll()
    if check and retcode:
        raise subprocess.CalledProcessError(
            retcode, process.args, output=stdout, stderr=stderr)
    return retcode, stdout, stderr

def createImageFromSessions(sessionData,saveDir,serverUrl):
    '''Function used to query all sessions on hgcentral'''
    processedSessionData = sessionData[1].split("\n")[:-1]
    imageFiles = []
    for session in processedSessionData[2:]:
        userName = session.split('\t')[0]
        sessionName = session.split('\t')[1]
        run(["wget","--output-file=/dev/null", "--output-document="+saveDir+"/"+userName+"."+sessionName+"-"+str(date.today())+".png", serverUrl+"/cgi-bin/hgRenderTracks?hgS_doOtherUser=submit&hgS_otherUserName="+userName+"&hgS_otherUserSessionName="+sessionName])
        imageFiles.append(userName+"."+sessionName+"-"+str(date.today())+".png")
    return(imageFiles)

def createImageFromSession(userName,sessionName,saveDir,serverUrl):
    '''Function for extacting only a single session'''
    run(["wget","--output-file=/dev/null", "--output-document="+saveDir+"/"+userName+"."+sessionName+"-"+str(date.today())+".png", serverUrl+"/cgi-bin/hgRenderTracks?hgS_doOtherUser=submit&hgS_otherUserName="+userName+"&hgS_otherUserSessionName="+sessionName])
    imageFile = userName+"."+sessionName+"-"+str(date.today())+".png"
    return(imageFile)

def populateServerDir1checkExist(userName,sessionName,serverDir1,serverUrl1):
    '''Creates all the image files for server 1 sessions if they do not exist in directory'''
    if os.path.exists(serverDir1) and os.path.isdir(serverDir1):
        if not os.listdir(serverDir1):
            print("No server one images found, populating directory")
            createImageFromSession(userName,sessionName,serverDir1,serverUrl1)
        else:    
            print("Archive images found")
    else:
        sys.exit("Sever one directory given does not exist")
        
def populateServerDir1(userName,sessionName,serverDir1,serverUrl1):
    '''Creates all the image files for server 1 regardless of existence or not'''
    if os.path.exists(serverDir1) and os.path.isdir(serverDir1):
        createImageFromSession(userName,sessionName,serverDir1,serverUrl1)
    else:
        sys.exit("Sever one directory given does not exist")

def imageCompare(imageFiles,serverDir2,serverDir1,diffImagesDir):
    '''Compare server1 and server2 generated images'''
    emptyFiles = 0
    imagesCompared = 0
    differentImages = 0
    diffImages = []
    noDiffImages = []
    for image in imageFiles:
        if os.path.getsize(serverDir2+"/"+image) == 0:
            emptyFiles+=1
        elif os.path.getsize(serverDir1+"/"+image) == 0:
            emptyFiles+=1
        else:    
            imagesCompared+=1
            previousImage = Image.open(serverDir1+"/"+image).convert('RGB')
            newImage = Image.open(serverDir2+"/"+image).convert('RGB')
            diff = ImageChops.difference(previousImage, newImage)
            if diff.getbbox():
                differentImages+=1
                diff.save(diffImagesDir+image)
                diffImages.append(image)
            else:
                noDiffImages.append(image)
    return(emptyFiles,imagesCompared,differentImages,diffImages,noDiffImages)
        
def reportOutput(emptyFiles,imagesCompared,differentImages,diffImages,noDiffImages,\
                 diffImagesDir,publicHtmlDirToSave,publicHtmlDirToView,serverUrl1,\
                 sessionUser1,sessionName1,serverUrl2,sessionUser2,sessionName2):
    '''Report findings, if differences found create symlinks to public html'''
    if noDiffImages != []:
        print("No differences seen in the following session(s):")
        for image in noDiffImages:
            print(image)
    if diffImages != []:
        print("\nDifferences were observed in the following session(s):")
        for image in diffImages:
            print(image)
            run(["ln", "-sf", diffImagesDir+image, publicHtmlDirToSave+image])
            print("Link: "+publicHtmlDirToView+image)
            print("session 1: %s/cgi-bin/hgTracks?hgS_doOtherUser=submit&hgS_otherUserName=%s&hgS_otherUserSessionName=%s"
                   %(serverUrl1,sessionUser1,sessionName1))
            print("session 1: %s/cgi-bin/hgTracks?hgS_doOtherUser=submit&hgS_otherUserName=%s&hgS_otherUserSessionName=%s" 
                   %(serverUrl2,sessionUser2,sessionName2))
    print("\nNumber of empty session files created: %s" % emptyFiles)
    print("Total number of images compared: %s" % imagesCompared)
    print("Different images found: %s" % differentImages)
    
###########
#To be used when extracting all session data
#sessionData = run(["hgsql", "-e", "select userName,sessionName from namedSessionDb", "hgcentraltest"], stdout=subprocess.PIPE)
##########

def main():
    user = getpass.getuser()    
    #Define cars
    serverUrl1 = "https://hgwdev.gi.ucsc.edu/"
    serverUrl2 = "https://hgwbeta.soe.ucsc.edu/"
    sessionUser1 = "lou"
    sessionUser2 = "lou"
    sessionName1 = "imageCompCron1.2"
    sessionName2 = "imageCompCron1.2"
    serverDir1 = "/hive/users/"+user+"/imageTest/cronImages/server1Images"
    serverDir2 = "/hive/users/"+user+"/imageTest/cronImages/server2Images"
    diffImagesDir = "/hive/users/"+user+"/imageTest/cronImages/diffImages/"
    publicHtmlDirToSave = "/cluster/home/"+user+"/public_html/images/"
    publicHtmlDirToView = "https://hgwdev.gi.ucsc.edu/~"+user+"/images/"
    #Create first set of image file(s)
    populateServerDir1(sessionUser1,sessionName1,serverDir1,serverUrl1)
    #Create second set of image file(s)
    imageFiles = []
    imageFiles.append(createImageFromSession(sessionUser2,sessionName2,serverDir2,serverUrl2))
    #Compare images
    emptyFiles,imagesCompared,differentImages,diffImages,noDiffImages = imageCompare(imageFiles,serverDir2,serverDir1,diffImagesDir)
    #Report findings
    if diffImages == []: #Check if there is anything to report - that way cron does not output
        pass
    else:
        reportOutput(emptyFiles,imagesCompared,differentImages,diffImages,noDiffImages,\
                     diffImagesDir,publicHtmlDirToSave,publicHtmlDirToView,serverUrl1,\
                     sessionUser1,sessionName1,serverUrl2,sessionUser2,sessionName2)
    
main()
