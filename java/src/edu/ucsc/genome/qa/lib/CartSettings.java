package edu.ucsc.genome.qa.lib;

/**
 *  This container holds cart settings.
 */ 
public class CartSettings {

  // data
  Position position;
  TrackControl trackControl;

  // constructors
 /**
  *  Constructor sets parameters for adding a track to the cart.
  *  
  *  @param  newPosition      The new browser position.
  *  @param  newTrackControl  The track to add to the cart.
  */ 
  public CartSettings(Position newPosition, TrackControl newTrackControl) {

    this.position = newPosition;
    this.trackControl = newTrackControl;
  }
}
