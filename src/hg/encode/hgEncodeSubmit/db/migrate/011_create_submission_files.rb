class CreateSubmissionFiles < ActiveRecord::Migration
  def self.up
    create_table :submission_files do |t|
      t.column :file_name, :string
      t.column :file_size, :string
      t.column :file_date, :timestamp
      t.column :submission_id, :integer
      t.column :type, :string
      t.column :status, :string
      t.column :created_at, :timestamp
      t.column :updated_at, :timestamp
    end
  end

  def self.down
    drop_table :submission_files
  end
end
