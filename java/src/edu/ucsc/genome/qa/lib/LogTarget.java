package edu.ucsc.genome.qa.lib;
import java.util.Properties;

/**
 *  This container holds properties used by a run of an Apache logging program
 */
public class LogTarget {

  // data

  public String sourceMachine;
  public String sourceDB;
  public String sourceTable;

  // could hold machine name or the string "all"
  public String targetMachine;
  // should make this a collection
  public int errorCode;
  // time interval
  public int minutes;

  // constructors

 /**
  * Constructor for parameters.
  * 
  * @param propFileName    File with properties
  */
  public LogTarget(String propFileName) {
    setDefaults();
    if (propFileName.equals("default")) {
      return;
    }

    Properties properties = new Properties();
    properties = QALibrary.readProps(propFileName);
    sourceMachine = properties.getProperty("sourceMachine", sourceMachine);
    sourceDB = properties.getProperty("sourceDB", sourceDB);
    sourceTable = properties.getProperty("sourceTable", sourceTable);
    targetMachine = properties.getProperty("targetMachine", targetMachine);
    String errorString = String.valueOf(errorCode);
    errorString = properties.getProperty("errorCode", errorString);
    errorCode = Integer.parseInt(errorString);
    String minuteString = String.valueOf(minutes);
    minuteString = properties.getProperty("minutes", minuteString);
    minutes = Integer.parseInt(minuteString);
  }
    
  public void setDefaults() {
    sourceMachine = "genome-log";
    sourceDB = "apachelog";
    sourceTable = "access_log";
    targetMachine = "all";
    errorCode = 500;
    minutes = 15;
 }

}
