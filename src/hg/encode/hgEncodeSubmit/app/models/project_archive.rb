class ProjectArchive < ActiveRecord::Base

  belongs_to :project
  has_many :project_files, :dependent => :destroy

  validates_presence_of :file_name
  validates_presence_of :file_size
  validates_presence_of :file_date
  validates_presence_of :project_id
  validates_presence_of :status
  #validates_presence_of :archives_active #causes problem for new upload

  protected
    def validate
      #errors.add(:price, "should be a positive value") if price.nil? || price < 0.01
    end

end
