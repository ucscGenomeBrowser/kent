class RenameTypeAndStatusFieldsFromSf < ActiveRecord::Migration
  def self.up
    remove_column :submission_files, :sf_type
    remove_column :submission_files, :status
  end

  def self.down
    add_column :submission_files, :sf_type, :string
    add_column :submission_files, :status, :string
  end
end
