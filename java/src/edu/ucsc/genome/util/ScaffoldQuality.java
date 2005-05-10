package edu.ucsc.genome.util;
import edu.ucsc.genome.qa.lib.*;
import java.sql.*;
import java.util.ArrayList;
import java.util.Iterator;


class ScaffoldQuality {
  public static void main(String args[]) {

    ArrayList scaffolds = new ArrayList();
    ArrayList contigs;
    String chromNameOld, chromName;
    String contigName;
    Contig contig;
    Scaffold scaffold, scaffoldNew, scaffoldOld;
    boolean firstTime = true;

    if (args.length != 2) {
      System.out.println("Please give database name and assembly table name");
      System.exit(1);
    }

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return; 

    // get access to database 
    HGDBInfo dbinfo; 
    try {
      dbinfo = new HGDBInfo("localhost", args[0]); 
    } catch (Exception e) { 
      System.out.println(e.toString()); 
      System.out.println("cannot connect to " + args[0]);
      return; 
    } 
    
    if (!dbinfo.validate()) return;
    try {
      String dbURL = QADBLibrary.jdbcURL(dbinfo);
      Connection con = DriverManager.getConnection(dbURL);

      // read in gold table
      Statement stmt = con.createStatement();
      chromNameOld = "";
      contig = new Contig("", 0, 0, 0, 0);
      scaffoldNew = new Scaffold("", 0, 0, contig);
      ResultSet rs = stmt.executeQuery("select chrom, chromStart, chromEnd, frag, fragStart, fragEnd from " + args[1]);
      while (rs.next()) {
        chromName = rs.getString("chrom");
        contigName = rs.getString("frag");
	int chromStart = rs.getInt("chromStart");
	int chromEnd = rs.getInt("chromEnd");
	int fragStart = rs.getInt("fragStart");
	int fragEnd = rs.getInt("fragEnd");
	contig = new Contig(contigName, chromStart, chromEnd, fragStart, fragEnd);
	if (!chromName.equals(chromNameOld)) {
	  if (!firstTime) {
	    scaffolds.add(scaffoldNew);
	  } else {
	    firstTime = false;
	  }
	  // System.out.println("Creating Scaffold object for " + chromName);
	  scaffoldNew = new Scaffold(chromName, chromStart, chromEnd, contig);
	  chromNameOld = chromName;
	  // System.out.println("chromNameOld = " + chromNameOld);
	} else {
	  scaffoldNew.addContig(contig);
	  scaffoldNew.updateEndPos(chromEnd);
	}
      }
      stmt.close();
      scaffolds.add(scaffoldNew);
      // System.out.println();

      Statement stmt2 = con.createStatement();
      Iterator scaffoldIter = scaffolds.iterator();
      while (scaffoldIter.hasNext()) {
        scaffold = (Scaffold) scaffoldIter.next();
	// scaffold.print();
	int expectedPos = 0;
	System.out.println("fixedStep chrom=" + scaffold.name + " start=1");
	contigs = scaffold.contigs;
	// handle starting gaps
	if (scaffold.startPos > expectedPos) {
	  for (int i = 0; i < expectedPos; i++) 
	    System.out.println("0");
        }
	expectedPos = scaffold.startPos;

	// iterate through contigs
	// store scores in ArrayList
	Iterator contigIter = contigs.iterator();
	ArrayList scoreArray = new ArrayList();
	while (contigIter.hasNext()) {
	  contig = (Contig) contigIter.next();
	  // handle gaps
	  if (contig.chromStart > expectedPos) {
	    for (int j = expectedPos; j <= contig.chromStart; j++)
	      System.out.println("0");
	  }

	  expectedPos = contig.chromEnd + 1;
	  scoreArray.clear();
	  String query = "select scores from contigQuality where name = '" + contig.contigName + "'";
	  ResultSet rs2 = stmt2.executeQuery(query);
	  while (rs2.next()) {
            Blob myblob = rs2.getBlob("scores");
            long blobLen = myblob.length();
            byte [] data = myblob.getBytes(1, (int)blobLen);
	    String scoreString = "";
            for (int i = 0; i < blobLen; i++) {
              char c = (char) data[i];
	      if (c == ' ') 
	      {
	        scoreArray.add(scoreString);
	        scoreString = "";
	      }
	      else
	      {
	        Character fullChar = new Character(c);
	        scoreString = scoreString + fullChar.toString();
	      }
            }
	    if (!scoreString.equals(""))
	      scoreArray.add(scoreString);
	  }


	  // check 
	  int contigMax = contig.chromEnd - contig.chromStart;
	  int scoreCount = scoreArray.size();
	  if (scoreCount < contigMax) {
	    System.out.println("warning: contig " + contig.contigName + " has size " + contigMax);
	    System.out.println("but only " + scoreCount + " scores.  (Padding with zeroes)");
	    for (int j = scoreCount; j < contigMax; j++)
	      scoreArray.add("0");
	  } 
	  
	  // else if (scoreCount > contigMax) {
	    // System.out.println("warning: extra scores in " + contig.contigName);
	    // System.out.println(scoreCount + " scores but contigMax is only " + contigMax );
	  // }
	  
	  // print
	  for (int k = contig.fragStart; k < contig.fragEnd; k++)
	    System.out.println(scoreArray.get(k));
	  expectedPos = contig.chromEnd + 1;
	}
	// this is where I would check for ending gaps
	// to do that, I would need to read gap table
	// if (expectedPos < scaffold.endPos)
	  // for (int k = expectedPos; k < scaffold.endPos; k++)
	    // System.out.println("0");
      }
    } catch (Exception e) { 
      System.out.println(e.toString()); 
      System.out.println("cannot connect to " + args[1]);
      return; 
    } 
  }
}

