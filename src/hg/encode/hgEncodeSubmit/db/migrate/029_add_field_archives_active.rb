class AddFieldArchivesActive < ActiveRecord::Migration
  def self.up
   add_column :projects, :archives_active, :text
   add_column :project_archives, :archives_active, :text
  end

  def self.down
   remove_column :projects, :archives_active
   remove_column :project_archives, :archives_active
  end
end
