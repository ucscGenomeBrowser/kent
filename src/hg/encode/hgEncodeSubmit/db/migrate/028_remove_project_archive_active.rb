class RemoveProjectArchiveActive < ActiveRecord::Migration
  def self.up
    drop_table :project_archive_actives
  end

  def self.down
  end
end
