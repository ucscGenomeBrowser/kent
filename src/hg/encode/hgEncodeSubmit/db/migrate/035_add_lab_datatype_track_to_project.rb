class AddLabDatatypeTrackToProject < ActiveRecord::Migration
  def self.up
     add_column :projects, :lab, :string
     add_column :projects, :data_type, :string
     add_column :projects, :track, :string
  end

  def self.down
     remove_column :projects, :lab
     remove_column :projects, :data_type
     remove_column :projects, :track
  end
end
