import sys
import os
import difflib
import unittest
import RaFile

def DiffExpected(outStr, expFileName, outFileName):
    file = open(os.getcwd() + '/tests/' +  expFileName)
    outfile = open(os.getcwd() + '/testoutput/' + outFileName, 'w')
    expected = file.read()

    outfile.write('========Expected========\n')
    outfile.write(expected)
    outfile.write('=========Output=========\n')
    outfile.write(outStr)
    outfile.write('========================\n')

    diffstr = ''
    for line in difflib.unified_diff(expected, outStr, 'expected', 'out'):
        diffstr += line
        outfile.write(line)

    outfile.close()
    return (len(diffstr) > 0)


class DiffCheck(unittest.TestCase):

    def testBasicDiff(self):                          
        """diffs basic input for core ra functionality"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/BasicDiff.ra')
        self.failIf(DiffExpected(ra.__str__(), 
            'BasicDiff.ra',
            'BasicDiff.out'))


    def testCommentsDiff(self):
        """diff to ensure that comments are preserved"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/CommentsDiff.ra')
        self.failIf(DiffExpected(ra.__str__(),
            'CommentsDiff.ra',
            'CommentsDiff.out'))
        

#    def testInlineCommentsDiff(self):
#        """diff to ensure that inline comments are preserved"""
#        ra = RaFile.RaFile(RaFile.RaEntry)
#        ra.read(os.getcwd() + '/tests/InlineCommentsDiff.ra')
#        self.failIf(DiffExpected(ra.__str__(),
#            'InlineCommentsDiff.ra',
#            'InlineCommentsDiff.out'))
        

class InvalidFilesCheck(unittest.TestCase):

    def testDuplicateKeys(self):
        """makes sure that duplicate keys are caught"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        self.assertRaises(KeyError, ra.read, os.getcwd() + '/tests/DuplicateKeys.ra')


    def testMisplacedKeys(self):
        """checks if keys in the incorrect place are caught"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        self.assertRaises(KeyError, ra.read, os.getcwd() + '/tests/MisplacedKeys.ra')


    def testNonExistentFile(self):
        """checks if invalid files are caught"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        self.assertRaises(IOError, ra.read, os.getcwd() + '/tests/FileDoesNotExist.ra')


    def testNonNewlineFile(self):
        """ensures file ends in newline"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        self.assertRaises(IOError, ra.read, os.getcwd() + '/tests/NonNewlineFile.ra')


#    def testInvalidComments(self):
#        """ensures file doesn't have comments in the middle of stanzas"""
#        ra = RaFile.RaFile(RaFile.RaEntry)
#        self.assertRaises(KeyError, ra.read, os.getcwd() + '/tests/InvalidComments.ra')


class FunctionalityCheck(unittest.TestCase):
 
    def testIterKeys(self):
        """tests that iterkeys works"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/FunctionalityCheck.ra')
       
        outstr = ''
        for entry in ra.iterkeys():
            outstr += str(entry) + '\n'

        self.failIf(DiffExpected(outstr,
            'IterKeys.ra',
            'IterKeys.out'))
       

    def testIterValues(self):
        """tests that itervalues works"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/FunctionalityCheck.ra')

        outstr = ''
        for entry in ra.itervalues():
            outstr += str(entry) + '\n'

        self.failIf(DiffExpected(outstr,
            'IterValues.ra',
            'IterValues.out'))


    def testIterItems(self):
        """tests that iteritems works"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/FunctionalityCheck.ra')

        outstr = ''
        for entry in ra.iteritems():
            outstr += str(entry) + '\n'

        self.failIf(DiffExpected(outstr,
            'IterItems.ra',
            'IterItems.out'))


    def testIterEntryKeys(self):
        """tests that iterkeys works"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/FunctionalityCheck.ra')

        outstr = ''
        for entry in ra.itervalues():
            for item in entry.iterkeys():
                outstr += str(item) + '\n'

        self.failIf(DiffExpected(outstr,
            'IterEntryKeys.ra',
            'IterEntryKeys.out'))


    def testIterEntryValues(self):
        """tests that iterkeys works"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/FunctionalityCheck.ra')

        outstr = ''
        for entry in ra.itervalues():
            for item in entry.itervalues():
                outstr += str(item) + '\n'

        self.failIf(DiffExpected(outstr,
            'IterEntryValues.ra',
            'IterEntryValues.out'))


    def testIterEntryItems(self):
        """tests that iterkeys works"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/FunctionalityCheck.ra')

        outstr = ''
        for entry in ra.itervalues():
            for item in entry.iteritems():
                outstr += str(item) + '\n'

        self.failIf(DiffExpected(outstr,
            'IterEntryItems.ra',
            'IterEntryItems.out'))


    def testGetItem(self):
        """tests that getting stanzas works"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/FunctionalityCheck.ra')

        outstr = ''
        outstr += str(ra['valA']) + '\n'

        self.failIf(DiffExpected(outstr,
            'GetItem.ra',
            'GetItem.out'))


    def testDeleteItem(self):
        """tests that removing items works"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/FunctionalityCheck.ra')

        outstr = ''
        del ra['valA']
        outstr = str(ra)

        self.failIf(DiffExpected(outstr,
            'DeleteItem.ra',
            'DeleteItem.out'))


if __name__ == '__main__':
    unittest.main()
