class PipelineController < ApplicationController

  require 'open-uri'

  before_filter :login_required
  before_filter :check_user_is_owner, :except => [:new, :create, :list, :show_user ]
  
  layout 'standard'
  
  def list
    @submissions = Submission.find(:all)
  end
  
  def show_user
    @user = User.find(current_user.id)
    @submissions = @user.submissions
    render :action => 'list'
    # not now using show_user.rhtml
  end
  
  def show
    @submission = Submission.find(params[:id])
  end

  def new
    #@users = User.find(:all)
    @submission = Submission.new
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
    submissionDir=File.expand_path("#{RAILS_ROOT}/upload/#{@current_user.id}/#{@submission.id}/")
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

    # make sure parent paths exist
    submissionDir = File.dirname(path_to_file)
    userDir = File.dirname(submissionDir)
    Dir.mkdir(userDir) unless File.exists?(userDir)
    Dir.mkdir(submissionDir) unless File.exists?(submissionDir)

    msg = ""
    # clean out directory (this will be handled more delicately later for re-uploading a submission)
    Dir.entries(submissionDir).each { 
      |f| 
      #msg += "submissionDir: #{f} #{File.ftype(File.join(submissionDir,f))}<br>\n" 
      fullName = File.join(submissionDir,f)
      if File.ftype(fullName) == "file"
        File.delete(fullName)
      end
    }

    unless @upurl.blank?
      File.open(path_to_file, "wb") { |f| f.write(@upload.read) }
    else
      if @upload.instance_of?(Tempfile)
        FileUtils.copy(@upload.local_path, path_to_file)
      else
        File.open(path_to_file, "wb") { |f| f.write(@upload.read) }
      end
    end
    # save filename in the database
    #write_attribute("file", path_to_file)

    #debugging
    msg += "content_type=#{@upload.content_type}<br>"
    msg += "original_filename=#{@upload.original_filename}<br>" if @upurl.blank?
    msg += "sanitized filename=#{@filename}<br>"
    msg += "size=#{@upload.size}<br>"
    msg += "RAILS_ROOT=#{RAILS_ROOT}<br>"
    msg += "path_to_file=#{path_to_file}<br>"

    # handle unzipping the archive
    case @upload.content_type.chomp
    when "application/zip"
      cmd = "unzip -j -o  #{path_to_file} -d #{submissionDir}"     # .zip 
    when "application/x-tar"
      if ["gz", "GZ"].any? {|ext| @filename.ends_with?("." + ext) }
        cmd = "tar --no-recursion -xzf #{path_to_file} -C #{submissionDir}"  # .gz  gzip
      else  
        cmd = "tar --no-recursion -xjf #{path_to_file} -C #{submissionDir}"  # .bz2 bzip2
      end
    end


    # -- dead code (assuming new fork/wait/kill works)
    #exitCode = system(cmd)
    #msg += "unzip exitCode = #{exitCode}<br>"  
    #unless exitCode 
    #  flash[:notice] = msg 
    #  return
    #end

    exitCode = run_with_timeout(cmd, 8)
    if exitCode >= 0
      msg += "unzip exitCode = #{exitCode}<br>" 
    else
      flash[:warning] = "unzip timeout exceeded<br>"  
      return
    end 

    # delete any dependent submissionFile records
    # Site.find(@params['id']).destroy
    @submission.submission_files.each do |c|
      c.destroy
    end

    # process the archive files
    msg += "submissionDir:<br>\n" 
    Dir.entries(submissionDir).each { 
      |f| 
      fullName = File.join(submissionDir,f)
      if File.ftype(fullName) == "file"
        if fullName == path_to_file
          msg += "&nbsp;#{f} (archive)<br>\n" 
        else
          msg += "&nbsp;#{f}<br>\n"
          unless ["bed", "idf", "adf", "sdrf" ].any? {|ext| f.downcase.ends_with?("." + ext) }
            flash[:warning] = "unknown file type: #{f}"
            return
          end 
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
        end
      end
    }

    @submission.file_name = @filename
    @submission.file_size = @upload.size
    @submission.file_date = Time.now    # TODO: add .utc to make UTC time?
    @submission.status = "uploaded"

    if @submission.save
      msg += "saved ok.<br>\n" 
      flash[:notice] = msg
      redirect_to :action => 'list'
    else
      flash[:warning] = "submission record save failed"
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
    File.expand_path("#{RAILS_ROOT}/upload/#{@current_user.id}/#{@submission.id}/#{@filename}")
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
