class RenameSubTypeForSubmissions < ActiveRecord::Migration
  def self.up
    rename_column :submissions, :sub_type, :submission_type_id
  end

  def self.down
    rename_column :submissions, :submission_type_id, :sub_type
  end
end
