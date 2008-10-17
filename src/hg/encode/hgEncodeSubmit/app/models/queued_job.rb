class QueuedJob < ActiveRecord::Base

  #belongs_to :project
  #has_many :project_files, :dependent => :destroy

  validates_presence_of :project_id
  validates_presence_of :source_code
  validates_presence_of :queued_at

  protected
    def validate
      #errors.add(:price, "should be a positive value") if price.nil? || price < 0.01
    end

end
