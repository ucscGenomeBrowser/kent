class ProjectFile < ActiveRecord::Base

  belongs_to :project_archive

  validates_presence_of :file_name
  validates_presence_of :file_size
  validates_presence_of :file_date
  validates_presence_of :project_archive_id

  protected
    def validate
      #errors.add(:price, "should be a positive value") if price.nil? || price < 0.01
    end

end
