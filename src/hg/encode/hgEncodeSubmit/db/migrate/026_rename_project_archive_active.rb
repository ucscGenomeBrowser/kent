class RenameProjectArchiveActive < ActiveRecord::Migration
  def self.up
    rename_table :project_archive_active, :project_archive_actives
  end

  def self.down
    rename_table :project_archive_actives, :project_archive_active
  end
end
