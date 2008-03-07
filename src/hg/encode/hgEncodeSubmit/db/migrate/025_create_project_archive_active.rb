class CreateProjectArchiveActive < ActiveRecord::Migration
  def self.up
    create_table :project_archive_active do |t|
        t.column :archive_id, :integer
        t.column :archive_no, :integer
        t.column :active, :boolean
    end

    execute "insert into project_archive_active (archive_id, archive_no, active) select a.id, b.archive_no, true from project_archives a, project_archives b where a.project_id = b.project_id and a.archive_no >= b.archive_no order by a.id, b.archive_no"

  end

  def self.down
    drop_table :project_archive_active
  end
end
