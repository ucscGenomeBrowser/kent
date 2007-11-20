class RenameSubmissionToProject < ActiveRecord::Migration
  def self.up
    rename_table :submissions, :projects
    rename_table :submission_archives, :project_archives
    rename_table :submission_files, :project_files
    rename_table :submission_types, :project_types
    rename_column :projects, :submission_type_id, :project_type_id
    rename_column :project_archives, :submission_id, :project_id
    rename_column :project_files, :submission_archive_id, :project_archive_id
  end

  def self.down
    rename_column :projects, :project_type_id, :submission_type_id
    rename_column :project_archives, :project_id, :submission_id
    rename_column :project_files, :project_archive_id, :submission_archive_id
    rename_table :projects, :submissions
    rename_table :project_archives, :submission_archives
    rename_table :project_files, :submission_files
    rename_table :project_types, :submission_types
  end
end
