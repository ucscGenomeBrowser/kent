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
      contig = new Contig("", 0, 0);
      scaffoldOld = new Scaffold("", 0, 0, contig);
      scaffoldNew = new Scaffold("", 0, 0, contig);
      ResultSet rs = stmt.executeQuery("select chrom, chromStart, chromEnd, frag, fragStart, fragEnd from " + args[1]);
      while (rs.next()) {
        chromName = rs.getString("chrom");
        contigName = rs.getString("frag");
	int chromStart = rs.getInt("chromStart");
	int chromEnd = rs.getInt("chromEnd");
	int fragStart = rs.getInt("fragStart");
	int fragEnd = rs.getInt("fragEnd");
	contig = new Contig(contigName, fragStart, fragEnd);
	if (!chromName.equals(chromNameOld)) {
	  if (!firstTime) {
	    scaffoldOld = scaffoldNew;
	    scaffolds.add(scaffoldOld);
	  } else {
	    firstTime = false;
	  }
	  // System.out.println("Creating Scaffold object for " + chromName);
	  scaffoldNew = new Scaffold(chromName, chromStart, chromEnd, contig);
	  chromNameOld = chromName;
	  // System.out.println("chromNameOld = " + chromNameOld);
	} else {
	  scaffoldNew.addContig(contig);
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
        firstTime = true;
	System.out.println("fixedStep chrom=" + scaffold.name + " start=1");
	contigs = scaffold.contigs;
	// handle starting gaps
	if (firstTime) {
	  if (scaffold.startPos > 0) {
	    for (int i = 0; i < scaffold.startPos; i++) 
	      System.out.println("0");
	  }
	  firstTime = false;   
        }

	// iterate through contigs
	// store scores in ArrayList
	Iterator contigIter = contigs.iterator();
	ArrayList scoreArray = new ArrayList();
	while (contigIter.hasNext()) {
	  contig = (Contig) contigIter.next();
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
	  int contigMax = contig.endPos;
	  int scoreCount = scoreArray.size();
	  if (scoreCount < contigMax) {
	    System.out.println("warning: contig " + contig.contigName + " has endPos " + contigMax);
	    System.out.println("but only " + scoreCount + " scores.  (Padding with zeroes)");
	    for (int j = scoreCount; j < contigMax; j++)
	      scoreArray.add("0");
	  } 
	  
	  // else if (scoreCount > contigMax) {
	    // System.out.println("warning: extra scores in " + contig.contigName);
	    // System.out.println(scoreCount + " scores but contigMax is only " + contigMax );
	  // }
	  
	  // print
	  for (int k = contig.startPos; k < contig.endPos; k++)
	    System.out.println(scoreArray.get(k));
	}
      }
    } catch (Exception e) { 
      System.out.println(e.toString()); 
      System.out.println("cannot connect to " + args[1]);
      return; 
    } 
  }
}

