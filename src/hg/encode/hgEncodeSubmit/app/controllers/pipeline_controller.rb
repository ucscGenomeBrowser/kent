class PipelineController < ApplicationController

  require 'open-uri'

  before_filter :login_required
  before_filter :check_user_is_owner, :except => [:new, :create, :list, :show_user, :show ]
  
  layout 'standard'
  
  def list
    @submissions = Submission.find(:all)
    @submissionTypes = getSubmissionTypes
  end
  
  def show_user
    @user = User.find(current_user.id)
    @submissions = @user.submissions
    @submissionTypes = getSubmissionTypes
    render :action => 'list'
    # not now using show_user.rhtml
  end
  
  def show
    @submission = Submission.find(params[:id])
    @submissionTypes = getSubmissionTypes
  end

  def new
    @submission = Submission.new
    @submissionTypes = getSubmissionTypes
    @submissionTypesArray = @submissionTypes.to_a.sort_by { |x| x[1]["displayOrder"] }
  end
  
  def create
    @submission = Submission.new(params[:submission])
    @submission.user_id = @current_user.id 
    @submission.status = 'new'
    if @submission.save
      redirect_to :action => 'list'
    else
      render :action => 'new'
    end
  end
  
  def edit
    @submission = Submission.find(params[:id])
    @submissionTypes = getSubmissionTypes
    @submissionTypesArray = @submissionTypes.to_a.sort_by { |x| x[1]["displayOrder"] }
  end
  
  def update
    @submission = Submission.find(params[:id])
    if @submission.update_attributes(params[:submission])
      redirect_to :action => 'show', :id => @submission
    else
      render :action => 'edit'
    end
  end
  
  def delete
    submissionDir=File.expand_path("#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}/#{@current_user.id}/#{@submission.id}/")
    if File.exists?(submissionDir)
      Dir.entries(submissionDir).each { 
        |f| 
        fullName = File.join(submissionDir,f)
        if File.ftype(fullName) == "file"
          File.delete(fullName)
        end
      }
      Dir.delete(submissionDir)
    end
    Submission.find(params[:id]).destroy
    redirect_to :action => 'list'
  end
 
  def upload
    @submission = Submission.find(params[:id])
    return unless request.post?
    @upload = params[:upload_file]
    @upurl = params[:upload_url]
    if @upurl == "http://"
      @upurl = ""
    end

    return if @upload.blank? && @upurl.blank?
    #raise @upload.inspect
    #flash[:notice] = "#{@upload.inspect}"

    unless @upurl.blank?
      @filename = sanitize_filename(@upurl)
      @upload = open(@upurl)
    else
      @filename = sanitize_filename(@upload.original_filename)
    end

    extensionsByMIME = {
      "application/zip" => ["zip", "ZIP"],
      "application/x-tar" => ["tar.gz", "TAR.GZ", "tar.bz2", "TAR.BZ2"]
    }
    extensions = extensionsByMIME[@upload.content_type.chomp]
    unless extensions && extensions.any? {|ext| @filename.ends_with?("." + ext) }
      flash[:warning] = "File name <strong>#{@filename}</strong> is invalid. " +
        "Only a compressed archive file (zip,bz2,gz) is allowed"
      # debug only next line:
      flash[:notice] = "content_type=#{@upload.content_type}" unless @upload.blank?
      return
    end


    msg = ""

    # make sure parent paths exist
    submissionDir = File.dirname(path_to_file)
    userDir = File.dirname(submissionDir)
    Dir.mkdir(userDir) unless File.exists?(userDir)
    Dir.mkdir(submissionDir) unless File.exists?(submissionDir)

    # make a temporary upload directory to unpack and merge from
    uploadDir = submissionDir+"/upload"
    if File.exists?(uploadDir)
      cmd = "rm -fr #{uploadDir}"
      exitCode = system(cmd)
      unless exitCode 
        msg += "command=[#{cmd}], exitCode = #{exitCode}<br>"  
        flash[:notice] = msg 
        return
      end
    end
    Dir.mkdir(uploadDir)


    nextArchiveNo = @submission.archive_count+1

    @filename = "#{"%03d" % nextArchiveNo}_#{@filename}"



    #####  DEAD CODE  #####
#    # clean out directory (this will be handled more delicately later for re-uploading a submission)
#    Dir.entries(submissionDir).each { 
#      |f| 
#      #msg += "submissionDir: #{f} #{File.ftype(File.join(submissionDir,f))}<br>\n" 
#      fullName = File.join(submissionDir,f)
#      if File.ftype(fullName) == "file"
#        File.delete(fullName)
#      end
#    }

    # just in case, remove it it already exists (shouldn't happen)
    File.delete(path_to_file) if File.exists?(path_to_file)

    unless @upurl.blank?
      File.open(path_to_file, "wb") { |f| f.write(@upload.read) }
    else
      if @upload.instance_of?(Tempfile)
        FileUtils.copy(@upload.local_path, path_to_file)
      else
        File.open(path_to_file, "wb") { |f| f.write(@upload.read) }
      end
    end

    # dead code, just an example of using write_attribute: save filename in the database
    #write_attribute("file", path_to_file)

    #debugging
    msg += "content_type=#{@upload.content_type}<br>"
    msg += "original_filename=#{@upload.original_filename}<br>" if @upurl.blank?
    msg += "sanitized filename=#{@filename}<br>"
    msg += "size=#{@upload.size}<br>"
    msg += "RAILS_ROOT=#{RAILS_ROOT}<br>"
    msg += "upload path=#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}<br>"
    msg += "path_to_file=#{path_to_file}<br>"
    msg += "nextArchiveNo=#{nextArchiveNo}<br>"


    # handle unzipping the archive
    case @upload.content_type.chomp
    when "application/zip"
      cmd = "unzip -j -o  #{path_to_file} -d #{uploadDir}"     # .zip 
    when "application/x-tar"
      if ["gz", "GZ"].any? {|ext| @filename.ends_with?("." + ext) }
        cmd = "tar --no-recursion -xzf #{path_to_file} -C #{uploadDir}"  # .gz  gzip
      else  
        cmd = "tar --no-recursion -xjf #{path_to_file} -C #{uploadDir}"  # .bz2 bzip2
      end
    end

    exitCode = run_with_timeout(cmd, 8)
    if exitCode >= 0
      msg += "unzip exitCode = #{exitCode}<br>" 
    else
      flash[:warning] = "unzip timeout exceeded<br>"  
      return
    end 

    # process the archive files
    msg += "uploadDir:<br>\n" 
    Dir.entries(uploadDir).each { 
      |f| 
      fullName = File.join(uploadDir,f)
      if File.ftype(fullName) == "file"
        msg += "&nbsp;#{f}<br>\n"
        unless ["bed", "idf", "adf", "sdrf" ].any? {|ext| f.downcase.ends_with?("." + ext) }
          flash[:warning] = "unknown file type: #{f}"
          return
        end 
    
        # delete any equivalent submissionFile records
        old = SubmissionFile.find(:first, :conditions => ['submission_id = ? and file_name = ?', @submission.id, f])
        old.destroy if old

        @submission_file = SubmissionFile.new
        @submission_file.file_name = f
        @submission_file.file_size = File.size(fullName)
        @submission_file.file_date = File.ctime(fullName)
        @submission_file.submission_id = @submission.id 
        @submission_file.sf_type = File.extname(f)
        @submission_file.status = 'uploaded'
        unless @submission_file.save
          flash[:warning] = "error saving submission_file record for: #{f}"
          return
        end
    
        toName = submissionDir + "/" + f    
        # move file from temporary upload dir into parent dir
        File.rename(fullName,toName);

      end
    }

    # need to test for and delete any with same archive_no (just in case?)
    old = SubmissionArchive.find(:first, :conditions => ['submission_id = ? and archive_no = ?', @submission.id, nextArchiveNo])
    old.destroy if old
    # add new submissionArchive record
    @submission_archive = SubmissionArchive.new
    @submission_archive.submission_id = @submission.id 
    @submission_archive.archive_no = nextArchiveNo
    @submission_archive.file_name = @filename
    @submission_archive.file_size = @upload.size
    @submission_archive.file_date = Time.now    # TODO: add .utc to make UTC time?
    unless @submission_archive.save
      flash[:warning] = "error saving submission_archive record for: #{@filename}"
      return
    end

    @submission.status = "uploaded"
    @submission.archive_count = nextArchiveNo

    if @submission.save
      msg += "saved ok.<br>\n" 
      flash[:notice] = msg
      redirect_to :action => 'show', :id => @submission
    else
      flash[:warning] = "submission record save failed"
    end

    # cleanup: delete temporary upload subdirectory
    if File.exists?(uploadDir)
      cmd = "rm -fr #{uploadDir}"
      exitCode = system(cmd)
      unless exitCode 
        msg += "command=[#{cmd}], exitCode = #{exitCode}<br>"  
        flash[:notice] = msg 
        return
      end
    end

    #@upload.methods.each {|x| msg += "#{x.to_str}<br>"}
    flash[:notice] = msg

  end
  
private
  def check_user_is_owner
    @submission = Submission.find(params[:id])
    unless @submission.user_id == @current_user.id
      flash[:warning] = "That submission does not belong to you."
      redirect_to :action => 'list'
    end
  end

  # --- file upload routines ---

  def sanitize_filename(file_name)
    # get only the filename, not the whole path (from IE)
    just_filename = File.basename(file_name) 
    # replace all non-alphanumeric, underscore or periods with underscore
    just_filename.gsub(/[^\w\.\_]/,'_') 
  end

  def path_to_file
    # the expand_path method resolves this relative path to full absolute path
    File.expand_path("#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}/#{@current_user.id}/#{@submission.id}/#{@filename}")
  end

  def getSubmissionTypes
    open("#{RAILS_ROOT}/config/submissionTypes.yml") { |f| YAML.load(f.read) }
  end

  # --- run process with timeout ---- (probably should move this to an application helper location)
  def run_with_timeout(cmd, myTimeout)
    # run process, kill it if exceeds specified timeout in seconds
    sleepInterval = 0.5  #seconds
    if ( (cpid = fork) == nil)
      exec(cmd)
    else
      before = Time.now
      while (true)
	pid, status = Process.wait2(cpid,Process::WNOHANG)
        if pid == cpid
          return status.exitstatus
        end
        if ( (Time.now - before) > myTimeout)
          Process.kill("ABRT",cpid)
	  pid, status = Process.wait2(cpid) # clean up zombies
          return -1
        end
        sleep(sleepInterval)
      end
    end
  end


end
