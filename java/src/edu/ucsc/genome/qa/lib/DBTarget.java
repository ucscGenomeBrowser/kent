package edu.ucsc.genome.qa.lib;
import java.util.Properties;

/**
 *  This container holds properties used to define a database
 */
public class DBTarget {

  // data

  public String sourceMachine;
  public String sourceDB;

  // constructors

 /**
  * Constructor for parameters.
  * 
  * @param propFileName    File with properties
  */
  public DBTarget(String propFileName) {
    setDefaults();
    if (propFileName.equals("default")) {
      return;
    }

    Properties properties = new Properties();
    properties = QALibrary.readProps(propFileName);
    sourceMachine = properties.getProperty("sourceMachine", sourceMachine);
    sourceDB = properties.getProperty("sourceDB", sourceDB);
  }
    
  public void setDefaults() {
    sourceMachine = "localhost";
    sourceDB = "hg18";
 }

}
