#!/hive/groups/encode/dcc/bin/python
import sys, os, shutil
import unittest 
from ucscgenomics import track, ra, soft, cv

sys.path.append('../')

import mkChangeNotes
import CurrentMkChangeNotes

database = 'hg19'
composite = 'wgEncodeTest'
comp = track.CompositeTrack(database, composite)
alphaDir = comp._alphaMdbDir
publicDir = comp._publicMdbDir
argsdict = {'database': database, 'composite': composite, 'releaseNew': '1', 'releaseOld': 'solo', 'loose': 0, 'ignore': 0, 'summary': 0}

class mkChangeNotesCheck(unittest.TestCase):
    def testRelease1(self):
        alphafile = 'release1test/wgEncodeTest.ra'
        shutil.copy(alphafile, alphaDir)
        argsdict['releaseNew'] = '1'
        argsdict['releaseOld'] = 'solo'
        real = CurrentMkChangeNotes.makeNotes(argsdict)
        test = mkChangeNotes.makeNotes(argsdict)
        self.assertListEqual(test.output,real.output)

    def testRelease2(self):
        alphafile = 'release2test/wgEncodeTest.ra'
        publicfile = 'release1test/wgEncodeTest.ra'
        shutil.copy(alphafile, alphaDir)
        shutil.copy(publicfile, publicDir)
        argsdict['releaseNew'] = '2'
        argsdict['releaseOld'] = '1'
        real = CurrentMkChangeNotes.makeNotes(argsdict)
        test = mkChangeNotes.makeNotes(argsdict)
        self.assertListEqual(test.output,real.output)

    def testRelease3(self):
        alphafile = 'release3test/wgEncodeTest.ra'
        publicfile = 'release2test/wgEncodeTest.ra'
        shutil.copy(alphafile, alphaDir)
        shutil.copy(publicfile, publicDir)
        argsdict['releaseNew'] = '3'
        argsdict['releaseOld'] = '2'
        real = CurrentMkChangeNotes.makeNotes(argsdict)
        test = mkChangeNotes.makeNotes(argsdict)
        self.assertListEqual(test.output,real.output)

    def testRelease4(self):
        alphafile = 'release4test/wgEncodeTest.ra'
        publicfile = 'release3test/wgEncodeTest.ra'
        shutil.copy(alphafile, alphaDir)
        shutil.copy(publicfile, publicDir)
        argsdict['releaseNew'] = '4'
        argsdict['releaseOld'] = '3'
        real = CurrentMkChangeNotes.makeNotes(argsdict)
        test = mkChangeNotes.makeNotes(argsdict)
        self.assertListEqual(test.output,real.output)


def main():

    #user this area to check if the test you're going to make will work
    #remember to change unittest.main() to main() if you're gonna do that.
    argsdict = {'database': 'hg19', 'composite': 'wgEncodeRikenCage', 'releaseNew': '1', 'releaseOld': 'solo', 'loose': 0, 'ignore': 1, 'summary': 1}
    notes = CurrentMkChangeNotes.makeNotes(argsdict)
    for i in notes.output:
        print i


if __name__ == '__main__':
    unittest.main()
