package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.sql.*;
import java.util.ArrayList;

/** 
 * 
 *  Database utilites for table queries
 */
public class QADBLibrary {

 /** 
  *  Get string with machine/database/user/password. 
  * 
  *  @return   Database URL with machine, database, user, password
  */
  public static String jdbcURL(HGDBInfo dbinfo) {
    String dbURL = "jdbc:mysql://" + dbinfo.machine;
    dbURL = dbURL + "/" + dbinfo.database;
    dbURL = dbURL + "?user=" + dbinfo.user;
    dbURL = dbURL + "&password=" + dbinfo.password;
    return dbURL;
  }


  /* 
   * Get number of rows in tablename that match condition
   */ 

 /** 
  *  Get number of rows in the input table that match the condition
  * 
  *  @param  dbinfo     The machine, database, user and password.
  *  @param  tablename  The table to query 
  *  @param  condition  The criterion for the serach 
  *
  *  @return   Number of rows that match the condition.
  *  @throws  SQLException
  */
  public static int rowcount(HGDBInfo dbinfo, String tablename, String condition) 
    throws Exception {

    String dbURL = jdbcURL(dbinfo);
    Connection con = DriverManager.getConnection(dbURL);
    Statement stmt = con.createStatement();

    String query = "SELECT COUNT(*) AS cnt FROM " + tablename;
    query = query + " WHERE " + condition;

    ResultSet rs = stmt.executeQuery(query);
    rs.next();
    int cnt = rs.getInt("cnt");

    stmt.close();
    con.close();

    return(cnt);
  }


  /* Query database for a single field in a single table. */

 /**
  *  @param  dbinfo    The machine, database, user and password.
  *  @param  query     The query string.  
  *     Must be fully formed and return single item.
  * 
  *  @return  The field to get 
  *  @throws  New Exception. "No results for query"
  */
  public static String quickQuery(HGDBInfo dbinfo, String query) throws Exception {

    String dbURL = jdbcURL(dbinfo);
    Connection con = DriverManager.getConnection(dbURL);
    Statement stmt = con.createStatement();
    ResultSet rs = stmt.executeQuery(query);
    if (!rs.next()) {
        throw new Exception("No results for " + query);
    }
    String result = rs.getString(1);
    stmt.close();
    con.close();
    return (result);

  }

 /**
  *  See if a table exists in the given database.
  *  @param  dbinfo    The machine, database, user and password.
  *  @param  table     The table.  
  * 
  *  @return  true if table exists.
  */
  public static boolean tableExists(HGDBInfo dbinfo, String table) throws Exception {
    boolean exists = false;
    String dbURL = jdbcURL(dbinfo);
    Connection con = DriverManager.getConnection(dbURL);
    Statement stmt = con.createStatement();
    try {
	ResultSet rs = stmt.executeQuery("select count(*) from " + table);
	if (rs.next()) {
	    exists = true;
	}
    } catch (Exception e) {
       // Do nothing, we'll just report table as not existing. 
    }
    stmt.close();
    con.close();
    return exists;
  }

 /**
  *  See if a database exists.
  *  @param  dbinfo    The machine, database, user and password.
  * 
  *  @return  true if table exists.
  */
  public static boolean databaseExists(HGDBInfo dbinfo) throws Exception {
    boolean exists = false;
    String dbURL = jdbcURL(dbinfo);
    try {
      Connection con = DriverManager.getConnection(dbURL);
      con.close();
      exists = true;
    } catch (Exception e) {
       // Do nothing, we'll just report database as not existing. 
    }
    return exists;
  }


 /**
  *  Get default position for database.
  *
  *  @param  dbinfo    The machine, database, user and password.
  *  @param  assembly  The genome assembly for which the default postion is needed.
  *
  *  @return  The default position for the database.
  *  @throws  SQLException
  */
  public static String getDefaultPosition(HGDBInfo dbinfo, String assembly) 
                                 throws Exception {

     String query = "SELECT defaultPos FROM dbDb WHERE name = "
		+ "'" + assembly + "'";
     return quickQuery(dbinfo, query);
  }

 /**
  * Get all genome databases.
  */
  public static ArrayList getGenomeDatabases(HGDBInfo metadbinfo) {
     return getColumnMatchingCondition(metadbinfo, "dbDb", "name", "active = 1");
  }
 
 /**
  * Return ArrayList with all databases if db parameter is "all",
  * else just the one. 
  */
  public static ArrayList getDatabaseOrAll(HGDBInfo metadbinfo, String db) {
     if (db.equals("all"))
         return getGenomeDatabases(metadbinfo);
     ArrayList dbList = new ArrayList();
     dbList.add(db);
     return dbList;
  }

      
  // get a row in the trackDb table that matches tablename
  // only expecting one row to match
 /**
  *  Get a row in the trackDb table that matches tablename
  *
  *  @param  extension  Developer "sandbox" name
  *  @param  dbinfo     The machine, database, user and password.
  *  @param  tablename  The table for which the trackDb entry is needed.
  *
  *  @return  The trackDb row for the table
  */
  public static TrackDb getTrackDb(String extension,
                            HGDBInfo dbinfo, String tablename) {
 
    TrackDb td = new TrackDb(tablename);

    String dbURL = jdbcURL(dbinfo);
    String query = "SELECT * FROM trackDb";
    if (!extension.equals("")) {
      query = query + extension;
    }
    query = query + " WHERE tableName = ";
    query = query + "'" + tablename + "'";

    try {

      Connection con = DriverManager.getConnection(dbURL);
      Statement stmt = con.createStatement();
      ResultSet rs = stmt.executeQuery(query);
      if (!rs.next()) {
        return (td);
      }

      String shortLabelVar = rs.getString("shortLabel");
      String typeVar = rs.getString("type");
      String longLabelVar = rs.getString("longLabel");
      int visibilityVar = rs.getInt("visibility");
      float priorityVar = rs.getFloat("priority");
      int colorRVar = rs.getInt("colorR");
      int colorGVar = rs.getInt("colorG");
      int colorBVar = rs.getInt("colorB");
      int altColorRVar = rs.getInt("altColorR");
      int altColorGVar = rs.getInt("altColorG");
      int altColorBVar = rs.getInt("altColorB");
      int useScoreVar = rs.getInt("useScore");
      int privateVar = rs.getInt("private");
      int restrictCountVar = rs.getInt("restrictCount");
      Blob restrictListVar = rs.getBlob("restrictList");
      Blob urlVar = rs.getBlob("url");
      Blob htmlVar = rs.getBlob("html");
      String grpVar = rs.getString("grp");
      int canPackVar = rs.getInt("canPack");
      Blob settingsVar = rs.getBlob("settings");

      td = new TrackDb (tablename, shortLabelVar, typeVar,
                        longLabelVar, visibilityVar, priorityVar,
                        colorRVar, colorGVar, colorBVar,
                        altColorRVar, altColorGVar, altColorBVar,
                        useScoreVar, privateVar, restrictCountVar,
                        restrictListVar, urlVar, htmlVar,
                        grpVar, canPackVar, settingsVar);

      stmt.close();
      con.close();
    
    } catch (Exception e) {
      System.err.println(e.getMessage());
    }

    return (td);
  }


 /**
  *  Update a blob column 
  *
  *  @param  con        Database connection.
  *  @param  newblob    The new contents of the blob field.
  *  @param  blobcolumn The name of the blob column to update.
  *  @param  tablename  The name of the table for which the blob is being updated.
  *  @param  whereColname       The name of the row for which the blob is being updated.
  *  @param  whereColval        The value of the row for which the blob is being updated.
  *
  *  @throws  SQLException
  */
  public static void blobInsert(Connection con, String newblob, String blobcolumn,
                               String tablename, String whereColname, String whereColval) 
                         throws Exception {

    // System.out.println("starting blobInsert");
    // convert String into blob
    byte [] data = newblob.getBytes();

    // get BLOB from database?
    String getBlob = "select " + blobcolumn + " from " + tablename;
    getBlob = getBlob + " where " + whereColname + " = '" + whereColval + "'";
    Statement stmt = con.createStatement();
    ResultSet blobRS = stmt.executeQuery(getBlob);
    blobRS.next();
    Blob myblob = blobRS.getBlob(blobcolumn);
    myblob.setBytes(1, data);

    String query = "UPDATE " + tablename + " SET " + blobcolumn + " = ? ";
    query = query + "WHERE " + whereColname + " = '" + whereColval + "'";

    // System.out.println(query);

    PreparedStatement pstmt = con.prepareStatement(query);
    pstmt.setBlob(1, myblob);
    pstmt.executeUpdate();
    stmt.close();
    // System.out.println("finished with blobInsert");
  }

 /**
  *  Get a list of chromosome sizes.
  *
  *  @param  dbinfo     The machine, database, user and password.
  *
  *  @return list of ChromInfo objects containing chrom and size fields.
  */
  public static ArrayList getChromInfo(HGDBInfo dbinfo) {

    ArrayList chromlist = new ArrayList();
    String dbURL = jdbcURL(dbinfo);
    String query = "SELECT chrom, size FROM chromInfo";

    try {

      Connection con = DriverManager.getConnection(dbURL);
      Statement stmt = con.createStatement();
      ResultSet rs = stmt.executeQuery(query);
      while (rs.next()) {
        String chromVar = rs.getString("chrom");
        int sizeVar = rs.getInt("size");
        ChromInfo ci = new ChromInfo (chromVar, sizeVar);
        chromlist.add(ci);
      }

      stmt.close();
      con.close();
    
    } catch (Exception e) {
      System.err.println(e.getMessage());
    }

    return (chromlist);

  }

 /** 
  *  Get gene names from kgProtMap table
  * 
  *  @param  dbinfo   The machine, database, user and password.
  *  @param  table    Table with gene names.
  *
  *  @return       List of all Proteome Gene qName entries.
  */
  public static ArrayList getPBGenes(HGDBInfo dbinfo, String table) {

    ArrayList pbList = new ArrayList();
    String    dbURL  = jdbcURL(dbinfo);
    String    query  = "SELECT qName FROM " + table;

    try {

      Connection con  = DriverManager.getConnection(dbURL);
      Statement  stmt = con.createStatement();
      ResultSet  rs   = stmt.executeQuery(query);
      while (rs.next()) {
        String qNameVar      = rs.getString("qName");
        pbList.add(qNameVar);
      }
      stmt.close();
      con.close();
    
    } catch (Exception e) {
      System.err.println(e.getMessage());
    }

    return (pbList);
  }

 /** 
  *  Get genes from Known Genes (or equivalent) table
  * 
  *  @param  dbinfo   The machine, database, user and password.
  *  @param  table    Table with gene names.
  *  @param  pb       True if proteome browser query (needs proteinID).
  *
  *  @return       List of all Known Genes.  Includes proteinID if pb is true.
  */
  public static ArrayList getGenes(HGDBInfo dbinfo, String table, boolean pb) {

    ArrayList kgList = new ArrayList();
    String    dbURL  = jdbcURL(dbinfo);
    String    query;
    if (pb) {
      query  = "SELECT name, chrom, txStart, txEnd, proteinID FROM " + table;
    }else{
      query  = "SELECT name, chrom, txStart, txEnd FROM " + table;
    }

    try {

      Connection con  = DriverManager.getConnection(dbURL);
      Statement  stmt = con.createStatement();
      ResultSet  rs   = stmt.executeQuery(query);
      while (rs.next()) {
        String nameVar      = rs.getString("name");
        String chromVar     = rs.getString("chrom");
        int txStartVar      = rs.getInt("txStart");
        int txEndVar        = rs.getInt("txEnd");
        String proteinIdVar = null;
        if (pb) {
          proteinIdVar = rs.getString("proteinID");
        }
        KnownGene kg;
        if (pb) {
          kg = new KnownGene (nameVar, chromVar, txStartVar, txEndVar, proteinIdVar);
        } else {
          kg = new KnownGene (nameVar, chromVar, txStartVar, txEndVar);
        }
        kgList.add(kg);
      }
      stmt.close();
      con.close();
    
    } catch (Exception e) {
      System.err.println(e.getMessage());
    }

    return (kgList);
  }

 /** 
  *  Get genes from Known Genes (or equivalent) table
  * 
  *  @param  dbinfo   The machine, database, user and password.
  *  @param  table    Table with gene names
  *
  *  @return       List of all Known Genes.
  */

  /*
  Replaced by method above.  Left here for the moment. -- kuhn
  public static ArrayList getGenes(HGDBInfo dbinfo, String table) {

    ArrayList kgList = new ArrayList();
    String dbURL = jdbcURL(dbinfo);
    String query = "SELECT name, chrom, txStart, txEnd FROM " + table;

    try {

      Connection con = DriverManager.getConnection(dbURL);
      Statement stmt = con.createStatement();
      ResultSet rs = stmt.executeQuery(query);
      while (rs.next()) {
        String nameVar = rs.getString("name");
        String chromVar = rs.getString("chrom");
        int txStartVar = rs.getInt("txStart");
        int txEndVar = rs.getInt("txEnd");
        KnownGene kg = new KnownGene (nameVar, chromVar, txStartVar, txEndVar);
        kgList.add(kg);
      }
      stmt.close();
      con.close();
    
    } catch (Exception e) {
      System.err.println(e.getMessage());
    }

    return (kgList);
  }
  */

 /** 
  *  Gets entire column from a table.
  * 
  *  @param  dbinfo   The machine, database, user and password.
  *  @param  table    The table to query.
  *  @param  column   The column to retrieve.
  *  @param  debug    Runs debugging if set to true.
  *
  *  @return       List of all elements of the column.
  */
  public static ArrayList getColumn(HGDBInfo dbinfo, String table, 
                             String column, boolean debug) {

    String id;
    ArrayList dblist = new ArrayList();
    ResultSet rs;
    
    String dbURL = jdbcURL(dbinfo);

    try {
      Connection con = DriverManager.getConnection(dbURL);
      Statement stmt = con.createStatement();

      String query;

      query = "SELECT " + column + " FROM " + table;

      rs = stmt.executeQuery(query);
    
      while (rs.next()) {
        id = rs.getString(column);
        if (debug) {
          System.out.println(id);
        }
        dblist.add(id);
      }

      stmt.close();
      con.close();
    
    } catch (Exception e) {
      System.err.println(e.getMessage());
    }

    return (dblist);

  }



 /** 
  *  Gets entire column from a table.
  * 
  *  @param  dbinfo   The machine, database, user and password.
  *  @param  table    The table to query.
  *  @param  column   The column to retrieve.
  *  @param  condition   The condition to match.
  *
  *  @return       List of all elements of the column.
  */
  public static ArrayList getColumnMatchingCondition(HGDBInfo dbinfo, 
                                                     String table, 
                                                     String column, 
                                                     String condition) {

    String id;
    ArrayList dblist = new ArrayList();
    ResultSet rs;
    
    String dbURL = jdbcURL(dbinfo);

    try {
      Connection con = DriverManager.getConnection(dbURL);
      Statement stmt = con.createStatement();

      String query;

      query = "SELECT " + column + " FROM " + table + " where " + condition;

      rs = stmt.executeQuery(query);
    
      System.out.println("database list: \n");
      while (rs.next()) {
        id = rs.getString(column);
        dblist.add(id);
	System.out.println(id);
      }
      System.out.println();

      stmt.close();
      con.close();
    
    } catch (Exception e) {
      System.err.println(e.getMessage());
    }

    return (dblist);

  }


  // check JDBC driver
 /**
  *  Checks for JDBC driver
  * 
  *  @return       True if JDBC Driver found.
  */
  public static boolean checkDriver() {
    try {
      Class.forName("com.mysql.jdbc.Driver");
    } catch (ClassNotFoundException e) {
      System.err.println("Can't get mysql jdbc driver");
      System.err.println("QADBLibrary line 299");
      return (false);
    }
    return (true);
  }


  // read in gold table
  public static ArrayList readGold(HGDBInfo dbinfo, String tableName) throws Exception {

    ArrayList scaffolds = new ArrayList();
    boolean firstTime = true;

    String dbURL = QADBLibrary.jdbcURL(dbinfo);
    Connection con = DriverManager.getConnection(dbURL);

    Statement stmt = con.createStatement();
    String chromNameOld = "";
    Contig contig = new Contig("", 0, 0, 0, 0);
    Scaffold scaffoldNew = new Scaffold("", 0, 0, contig);
    ResultSet rs = 
      stmt.executeQuery("select chrom, chromStart, chromEnd, frag, fragStart, fragEnd from " + tableName);
    while (rs.next()) {
      String chromName = rs.getString("chrom");
      String contigName = rs.getString("frag");
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
	scaffoldNew = new Scaffold(chromName, chromStart, chromEnd, contig);
	chromNameOld = chromName;
      } else {
        scaffoldNew.addContig(contig);
        scaffoldNew.updateEndPos(chromEnd);
      }
    }
    stmt.close();
    scaffolds.add(scaffoldNew);

    return (scaffolds);
  }


  // get scores for this contig
  public static ArrayList getScores(Contig contig, String tableName, Statement stmt2) throws Exception {

    ArrayList scoreArray = new ArrayList();

    String query = "select scores from " + tableName + " where name = '" + contig.contigName + "'";
    ResultSet rs = stmt2.executeQuery(query);

    while (rs.next()) {
      Blob myblob = rs.getBlob("scores");
      long blobLen = myblob.length();
      byte [] data = myblob.getBytes(1, (int)blobLen);
      String scoreString = "";
      for (int i = 0; i < blobLen; i++) {
        char c = (char) data[i];
        if (c == ' ') {
          scoreArray.add(scoreString);
          scoreString = "";
       } else {
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

    return(scoreArray);
  }

}
