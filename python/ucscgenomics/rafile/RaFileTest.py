import sys
import os
import difflib
import unittest
import RaFile

class DiffCheck(unittest.TestCase):

    def testBasicDiff(self):                          
        """diffs basic input for core ra functionality"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/BasicDiff.ra')
        file = open(os.getcwd() + '/tests/BasicDiff.ra')
        outfile = open(os.getcwd() + '/testoutput/BasicDiff.out', 'w')
        expected = file.read()

        outfile.write('========Expected========\n')
        outfile.write(expected)
        outfile.write('=========Output=========\n')
        outfile.write(ra.__str__())
        outfile.write('========================\n')
 
        diffstr = ''
        for line in difflib.unified_diff(expected, ra.__str__(), 'expected', 'out'):
            diffstr += line
            outfile.write(line)

        outfile.close()
        self.failIf(len(diffstr) > 0)  


    def testCommentsDiff(self):
        """diff to ensure that comments are preserved"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/CommentsDiff.ra')
        file = open(os.getcwd() + '/tests/CommentsDiff.ra')
        outfile = open(os.getcwd() + '/testoutput/CommentsDiff.out', 'w')
        expected = file.read()

        outfile.write('========Expected========\n')
        outfile.write(expected)
        outfile.write('=========Output=========\n')
        outfile.write(ra.__str__())
        outfile.write('========================\n')

        diffstr = ''
        for line in difflib.unified_diff(expected, ra.__str__(), 'expected', 'out'):
            diffstr += line
            outfile.write(line)

        outfile.close()
        self.failIf(len(diffstr) > 0)

    def testExtraneousWhitespaceDiff(self):
        """diff to ensure that extraneous whitespace is truncated"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        ra.read(os.getcwd() + '/tests/ExtraneousWhitespace.ra')
        file = open(os.getcwd() + '/tests/ExtraneousWhitespaceExpected.ra')
        outfile = open(os.getcwd() + '/testoutput/ExtraneousWhitespace.out', 'w')
        expected = file.read()

        outfile.write('========Expected========\n')
        outfile.write(expected)
        outfile.write('=========Output=========\n')
        outfile.write(ra.__str__())
        outfile.write('========================\n')

        diffstr = ''
        for line in difflib.unified_diff(expected, ra.__str__(), 'expected', 'out'):
            diffstr += line
            outfile.write(line)

        outfile.close()
        self.failIf(len(diffstr) > 0)


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


    def testInvalidComments(self):
        """ensures file doesn't have comments in the middle of stanzas"""
        ra = RaFile.RaFile(RaFile.RaEntry)
        self.assertRaises(KeyError, ra.read, os.getcwd() + '/tests/InvalidComments.ra')


if __name__ == '__main__':
    unittest.main()
