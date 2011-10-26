#!/hive/groups/encode/dcc/bin/python
import sys, os, shutil
import unittest 
from ucscgenomics import mkChangeNotes

import CurrentMkChangeNotes

database = 'encodeTest'
composite = 'wgEncodeTest'


argsdict = {'database': database, 'composite': composite, 'releaseNew': '1', 'releaseOld': 'solo', 'loose': 0, 'ignore': 0, 'summary': 0, 'specialMdb': None}

class mkChangeNotesEndToEndCheck(unittest.TestCase):
    def testRelease1(self):
        argsdict['releaseNew'] = '1'
        argsdict['releaseOld'] = 'solo'
        argsdict['specialMdb'] = 'wgEncodeTest1'
        real = CurrentMkChangeNotes.makeNotes(argsdict)
        test = mkChangeNotes.makeNotes(argsdict)
        self.assertListEqual(test.output,real.output)

    def testRelease2(self):
        argsdict['releaseNew'] = '2'
        argsdict['releaseOld'] = '1'
        argsdict['specialMdb'] = 'wgEncodeTest2'
        real = CurrentMkChangeNotes.makeNotes(argsdict)
        test = mkChangeNotes.makeNotes(argsdict)
        self.assertListEqual(test.output,real.output)

    def testRelease3(self):

        argsdict['releaseNew'] = '3'
        argsdict['releaseOld'] = '2'
        argsdict['specialMdb'] = 'wgEncodeTest3'
        real = CurrentMkChangeNotes.makeNotes(argsdict)
        test = mkChangeNotes.makeNotes(argsdict)
        self.assertListEqual(test.output,real.output)

    def testRelease4(self):

        argsdict['releaseNew'] = '4'
        argsdict['releaseOld'] = '3'
        argsdict['specialMdb'] = 'wgEncodeTest4'
        real = CurrentMkChangeNotes.makeNotes(argsdict)
        test = mkChangeNotes.makeNotes(argsdict)
        self.assertListEqual(test.output,real.output)

class mkChangeNotesFunctionTests(unittest.TestCase):
    def testCheckMetaDbForFiles(self):
        argsdict['releaseNew'] = '3'
        argsdict['releaseOld'] = '2'
        argsdict['specialMdb'] = 'wgEncodeTest3'
        real = CurrentMkChangeNotes.makeNotes(argsdict)
        test = mkChangeNotes.makeNotes(argsdict)
        self.assertEqual(test.checkMetaDbForFiles("test", "new"), real.checkMetaDbForFiles("test",  "new"))
        
def main():

    #user this area to check if the test you're going to make will work
    #remember to change unittest.main() to main() if you're gonna do that.
    argsdict = {'database': 'encodeTest', 'composite': 'wgEncodeTest', 'releaseNew': '1', 'releaseOld': 'solo', 'loose': 0, 'ignore': 1, 'summary': 0}
    argsdict['releaseNew'] = '1'
    argsdict['releaseOld'] = 'solo'
    argsdict['specialMdb'] = 'wgEncodeTest1'
    real = CurrentMkChangeNotes.makeNotes(argsdict)
    test = mkChangeNotes.makeNotes(argsdict)

    print real.output
    print test.output
if __name__ == '__main__':
    unittest.main()
