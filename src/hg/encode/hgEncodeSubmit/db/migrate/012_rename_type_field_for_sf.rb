class RenameTypeFieldForSf < ActiveRecord::Migration
  def self.up
    rename_column :submission_files, :type, :sf_type
  end

  def self.down
    rename_column :submission_files, :sf_type, :type
  end
end
