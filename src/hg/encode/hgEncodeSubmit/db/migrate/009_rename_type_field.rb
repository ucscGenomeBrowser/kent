class RenameTypeField < ActiveRecord::Migration
  def self.up
    rename_column :submissions, :type, :s_type
  end

  def self.down
    rename_column :submissions, :s_type, :type
  end
end
