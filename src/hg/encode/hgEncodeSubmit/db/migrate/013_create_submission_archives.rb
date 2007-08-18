class CreateSubmissionArchives < ActiveRecord::Migration
  def self.up
    create_table :submission_archives do |t|
      t.column :file_name, :string
      t.column :file_size, :string
      t.column :file_date, :timestamp
      t.column :submission_id, :integer
      t.column :created_at, :timestamp
      t.column :updated_at, :timestamp
    end

   # try to migrate any data forwards
   execute 'insert into submission_archives(file_name, file_size, file_date, submission_id, created_at, updated_at) select file_name, file_size, file_date, id, created_at, updated_at from submissions'

   remove_column :submissions, :file_name
   remove_column :submissions, :file_size
   remove_column :submissions, :file_date

  end

  def self.down

    add_column :submissions, :file_name, :string
    add_column :submissions, :file_size, :string
    add_column :submissions, :file_date, :timestamp

    # note: we can't really properly migrate the data backwards

    drop_table :submission_archives
  end
end
