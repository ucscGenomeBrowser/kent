class AddArchiveCountAndArchiveNo < ActiveRecord::Migration
  def self.up
    add_column :submissions, :archive_count, :integer, :default => 0 
    add_column :submission_archives, :archive_no, :integer 
  end

  def self.down
    remove_column :submissions, :archive_count
    remove_column :submission_archives, :archive_no
  end
end
