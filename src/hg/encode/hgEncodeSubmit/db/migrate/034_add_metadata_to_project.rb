class AddMetadataToProject < ActiveRecord::Migration
  def self.up
     add_column :projects, :metadata, :text
     add_column :projects, :count, :integer
  end

  def self.down
     remove_column :projects, :metadata
     remove_column :projects, :count
  end
end
