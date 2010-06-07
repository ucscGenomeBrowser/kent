class AddQueuedJobs < ActiveRecord::Migration
  def self.up
    create_table :queued_jobs do |t|
      t.column :project_id, :integer
      t.column :source_code, :text
      t.column :queued_at, :timestamp
    end
  end

  def self.down
    drop_table :queued_jobs
  end
end
