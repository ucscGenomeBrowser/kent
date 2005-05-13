package edu.ucsc.genome.qa.lib;
import java.util.Properties;

/**
 *  This container holds properties used by a run of the LiftOverTest program
 */
public class LiftOverTestTarget {

  // data
  public String database;
  public String table;
  public String server;

  // constructors

 /**
  * Constructor for database query parameters.
  * 
  * @param propFileName    File with properties
  */
  public LiftOverTestTarget(String propFileName) {
    setDefaults();
    if (propFileName.equals("default")) {
      return;
    }

    Properties properties = new Properties();
    properties = QALibrary.readProps(propFileName);
    database = properties.getProperty("database", database);
    server = properties.getProperty("server", server);
    table = properties.getProperty("table", table);
  }
    
  public void setDefaults() {
    database = "qa";
    server = "localhost";
    table = "liftOverTestCase";
 }

}
