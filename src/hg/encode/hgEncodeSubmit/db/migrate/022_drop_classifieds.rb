class DropClassifieds < ActiveRecord::Migration
  def self.up
    drop_table :classifieds
  end

  def self.down
  end
end
