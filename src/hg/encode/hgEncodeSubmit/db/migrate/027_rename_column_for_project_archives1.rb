class RenameColumnForProjectArchives1 < ActiveRecord::Migration
  def self.up
    rename_column :project_archive_actives, :archive_id, :project_archive_id
  end

  def self.down
    rename_column :project_archive_actives, :project_archive_id, :archive_id
  end
end
