class CreateProjectStatusLogs < ActiveRecord::Migration
  def self.up
    create_table :project_status_logs do |t|
      t.column :project_id, :integer
      t.column :status, :string
      t.column :created_at, :timestamp
    end
  end

  def self.down
    drop_table :project_status_logs
  end
end
