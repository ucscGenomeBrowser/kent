package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.text.*;
import java.util.*;

/**
 *  This holds a parsed-out hg.conf file.
 */
public class HgConf extends Properties {
  private void init(String fileName) throws Exception {
    LineNumberReader in = new LineNumberReader(new FileReader(fileName));
    try {
      String line;
      while ((line = in.readLine()) != null) {
	line = line.trim();
	if (line.length() == 0)
	  continue;
	if (line.charAt(0) == '#')
	  continue;
	int equalsIx = line.indexOf('=');
	if (equalsIx < 0)
	  throw new ParseException("No = ", in.getLineNumber());
	setProperty(line.substring(0,equalsIx), line.substring(equalsIx+1));
      }
    } finally {
      in.close();
    }
  }

  public HgConf(String fileName) throws Exception {
    init(fileName);
  }

  /** This constructor will look in ~/.hg.conf */
  public HgConf() throws Exception {
    String fileName = System.getenv("HGDB_CONF");
    if (fileName == null) {
      fileName = System.getProperty("user.home") + "/.hg.conf";
    }
    init(fileName);
  }

  static public void main(String args[]) throws Exception {
    HgConf conf = new HgConf();
    System.out.println(conf.toString());
  }
}
