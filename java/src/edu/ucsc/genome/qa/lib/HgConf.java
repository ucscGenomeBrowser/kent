package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.text.*;
import java.util.*;

/**
 *  This holds a parsed-out hg.conf file.
 */
public class HgConf extends Properties {
  private void parseFile(String fileName) throws Exception {
    LineNumberReader in = new LineNumberReader(new FileReader(fileName));
    try {
      String line;
      while ((line = in.readLine()) != null) {
         parseLine(fileName, in, line);
      }
    } finally {
      in.close();
    }
  }

  private void parseLine(String fileName, LineNumberReader in, String line) throws Exception {
    line = line.trim();
      if (! (line.length() == 0 || line.charAt(0) == '#')) {
        if (line.startsWith("include")) {
           parseFile(parseIncludeLine(fileName, line));
        } else {
          int equalsIx = line.indexOf('=');
          if (equalsIx < 0)
             throw new ParseException(fileName + ":" + Integer.toString(in.getLineNumber()) + ": no \"=\" found", in.getLineNumber());
          setProperty(line.substring(0,equalsIx), line.substring(equalsIx+1));
        }
      }
  }

  private String parseIncludeLine(String fileName, String line) throws Exception {
    // construct file path relative to current file path, unless included file path is absolute
    String[] words = line.split("\\s+");
    if (words.length != 2) {
      throw new Exception("invalid include line: " + line);
    }
    File includingFile = new File(fileName);
    File includedFile = new File(words[1]);
    if (includingFile.getParent()!=null && !includedFile.isAbsolute()) {
      // construct relative path
      return new File(includingFile.getParent(), includedFile.toString()).toString();
    } else {
      // use as-is
      return includedFile.toString();
    }
  }


  public HgConf(String fileName) throws Exception {
    parseFile(fileName);
  }

  /** This constructor will look in ~/.hg.conf */
  public HgConf() throws Exception {
    String fileName = System.getenv("HGDB_CONF");
    if (fileName == null) {
      fileName = System.getProperty("user.home") + "/.hg.conf";
    }
    parseFile(fileName);
  }

  static public void main(String args[]) throws Exception {
    HgConf conf = new HgConf();
    System.out.println(conf.toString());
  }
}
