class MoveSubmissionFileParentToArchive < ActiveRecord::Migration
  def self.up
    rename_column :submission_files, :submission_id, :submission_archive_id
  end

  def self.down
    rename_column :submission_files, :submission_archive_id, :submission_id
  end
end
