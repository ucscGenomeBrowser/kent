module PipelineBackground

# TODO maybe all of these can be private ?

#  protected
  def background_test
    #require 'pp'   # debug
    #logger.info(projectType.pretty_inspect)
    #logger.info "called PipelineBackground::background_test"
    yell "called PipelineBackground::background_test"
    #return 5
  end

  def yell(msg)
    # stupid simple logging:
    f = File.open(File.expand_path(File.dirname(__FILE__) + "/../log/yell.log"),"a")
    f.puts msg
    f.close
  end

  def clear_run_stat(project_id)
    project = Project.find(project_id)
    project.run_stat = nil
    saver project
  rescue ActiveRecord::RecordNotFound
  end

  def set_run_stat(project_id)
    unless project_id == 0
      project = Project.find(project_id)
      project.run_stat = "running"
      saver project
    end
  end

  def validate_background(project_id)

    project = Project.find(project_id)
    new_status project, "validating"
    projectType = getProjectType(project)
    projectDir = path_to_project_dir(project_id)
 
    cmd = "#{projectType['validator']} #{projectType['type_params']} #{projectDir} &> #{projectDir}/validate_error"
    timeout = projectType['time_out']

    exitCode = run_with_timeout(cmd, timeout)

    if exitCode == 0
      new_status project, "validated"
      load_background(project_id)
    else
      new_status project, "validate failed"
    end

  end 

    
  def load_background(project_id)

    project = Project.find(project_id)
    new_status project, "loading"

    projectType = getProjectType(project)
 
    projectDir = path_to_project_dir(project_id)
    cmd = "#{projectType['loader']} #{projectType['load_params']} #{projectDir} &> #{projectDir}/load_error"
    timeout = projectType['load_time_out']

    exitCode = run_with_timeout(cmd, timeout)

    if exitCode == 0
      project.status = "loaded"
      # send email notification
      if ActiveRecord::Base.configurations[RAILS_ENV]['emailOnLoad']
        user = User.find(project.user_id)
        UserNotifier.deliver_load_notification(user, project)
      end
    else
      project.status = "load failed"
    end
    new_status project, project.status

  end 

  def unload_background(project_id)
    # call an unload cleanup routine 
    #  (e.g. that can remove .wib symlinks from /gbdb/ to the submission dir)
    project = Project.find(project_id)
    projectDir = path_to_project_dir(project_id)
    msg = ""
    if File.exists?(projectDir)
      unless new_status project, "unloading"
        return
      end
    
      projectType = getProjectType(project)
 
      projectDir = path_to_project_dir(project_id)
      cmd = "#{projectType['unloader']} #{projectType['unload_params']} #{projectDir} &> #{projectDir}/unload_error"
      timeout = projectType['unload_time_out']

      #logger.info "GALT! cmd=#{cmd} timeout=#{timeout}"

      exitCode = run_with_timeout(cmd, timeout)

      if exitCode == 0
        if project.project_archives.length == 0
          new_status project, "new"
        else
          new_status project, "uploaded"
        end
      else
        yell "Project unload failed."
        new_status project, "unload failed"
        return
      end

    end    

  end
 
  def delete_background(project_id)
    project = Project.find(project_id)
    unload_background(project.id)
    delete_completion(project.id)
    new_status project, "deleted"
    destroyer project
  end
 

  def upload_background(project_id, upurl, upftp, upload, local_path, autoResume)

    project = Project.find(project_id)

    #puts "got to start of upload method"  # debug DEBUG remove

    # handle FTP stuff
    user = project.user
    fullPath = ActiveRecord::Base.configurations[RAILS_ENV]['ftpMount']+'/'+user.login

    unless upurl
      upurl = ""
    end
    unless upftp
      upftp = ""
    end
    unless upload
      upload = ""
    end
    unless local_path
      local_path = ""
    end

    unless upurl.blank?
      filename = sanitize_filename(upurl)
    else
      unless upftp.blank?
        filename = sanitize_filename(upftp)
      else
        filename = sanitize_filename(upload)
      end
    end

    #msg = ""

    # make sure parent paths exist
    projectDir = path_to_project_dir(project_id)
    Dir.mkdir(projectDir,0775) unless File.exists?(projectDir)

    nextArchiveNo = project.archive_count+1

    plainName = filename
    filename = "#{"%03d" % nextArchiveNo}_#{filename}"

    pf = path_to_file(project_id, filename)

    #debugging
    #msg += "sanitized filename=#{filename}<br>"
    #msg += "RAILS_ROOT=#{RAILS_ROOT}<br>"
    #msg += "upload path=#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}<br>"
    #msg += "path_to_file=#{pf}<br>"
    #msg += "nextArchiveNo=#{nextArchiveNo}<br>"

    #msg += "Uploading/expanding #{plainName}.<br>"

    saveProjectStatus = project.status
    unless new_status project, "uploading"
      return
    end

    if upload.blank?
      # just in case, remove it if it already exists 
      #  but currently this happens if local file upload
      #  because the client creates it before upload_background is called
      File.delete(pf) if File.exists?(pf)
    end
    
    if project.project_archives.last
      project.project_archives.last.status = saveProjectStatus
      project.project_archives.last.archives_active = project.archives_active
      unless saver project.project_archives.last
        return
      end
    end

    unless upurl.blank?

      # trying for more info, remove -nv option:
      cmd = "wget -O #{pf} #{autoResume} '#{upurl}' &> #{projectDir}/upload_error" 

      #yell "\n\nGALT! cmd=[#{cmd}]\n\n"   # DEBUG remove

      timeout = 36000  # 10 hours
      exitCode = run_with_timeout(cmd, timeout)
      if exitCode != 0
        new_status project, "upload failed"
        return
      end

    else 
      unless upftp.blank?
        ftpPath = File.join(fullPath,upftp)
        unless File.exists?(ftpPath)
          cmd = "echo 'FTP file #{upftp} is missing' &> #{projectDir}/upload_error" 
          system(cmd)
          new_status project, "upload failed"
          return
        end
        #FileUtils.copy(ftpPath, pf)
        File.rename(ftpPath, pf)
        File.delete(ftpPath) if File.exists?(ftpPath) and File.exists(pf)  # just in case rename doesn't cleanup
      else
        unless local_path.blank?
          unless File.exists?(local_path)
            cmd = "echo 'local_path file #{local_path} is missing' &> #{projectDir}/upload_error" 
            system(cmd)
            new_status project, "upload failed"
            return
          end
          #FileUtils.copy(local_path, pf)
          File.rename(local_path, pf)
          File.delete(local_path) if File.exists?(local_path) and File.exists(pf)  # just in case rename doesn't cleanup
        end
      end
    end

    unless new_status project, "expanding"
      return
    end

    nextArchiveNo = project.archive_count+1
    if prep_one_archive(project, filename, nextArchiveNo)
      if process_uploaded_archive(project, filename)
        project.status = "uploaded"
      else
        project.status = "upload failed"
      end
    else
        project.status = "upload failed"
    end

    unless new_status project, project.status
      return
    end

    if project.status == "uploaded"
      validate_background(project_id)
    end

  end

 def process_uploaded_archive(project, filename)
    
    # make sure parent paths exist
    projectDir = path_to_project_dir(project.id)

    nextArchiveNo = project.archive_count+1

    pf = path_to_file(project.id, filename)

    # need to test for and delete any with same archive_no (just in case?)
    # moved this up here just to get the @project_archive.id set
    old = ProjectArchive.find(:first, :conditions => ['project_id = ? and archive_no = ?', project.id, nextArchiveNo])
    old.destroy if old
    # add new projectArchive record
    project_archive = ProjectArchive.new
    project_archive.project_id = project.id 
    project_archive.archive_no = nextArchiveNo
    project_archive.file_name = filename
    project_archive.file_size = File.size(pf)
    project_archive.file_date = Time.now    # TODO: add .utc to make UTC time?
    project_archive.status = "see current"
    project_archive.archives_active = ""
    unless saver project_archive
      return false
    end

    project.archive_count = nextArchiveNo
    project.archives_active += "1"

    unless saver project
      return false
    end

    unless expand_archive(project, project_archive)
      project.archive_count = nextArchiveNo - 1
      new_status project, "expand failed"
      return false
    end

    return true

  end


  def expand_archive(project, archive)
    # expand archive belonging to archive record

    projectDir = path_to_project_dir(project.id)
    uploadDir = projectDir+"/upload_#{archive.archive_no}"

    process_archive(project, archive.id, projectDir, uploadDir, "")

    # cleanup: delete temporary upload subdirectory
    clean_out_dir uploadDir

    if project.project_archives.length == 0
      project.status = "new"
    else
      project.status = "uploaded"
    end

    new_status project, project.status
    return true

  end

  def process_archive(project, archive_id, projectDir, uploadDir, relativePath)
    # process the archive files
    fullPath = my_join(uploadDir,relativePath)
    Dir.entries(fullPath).each do
      |f| 
      fullName = my_join(fullPath,f)
      if (File.ftype(fullName) == "directory")
        if (f != ".") and (f != "..")
          newRelativePath = my_join(relativePath,f)
          newDir = my_join(projectDir, newRelativePath)
	  unless File.exists?(newDir)
	    Dir.mkdir(newDir,0775)
	  end
          process_archive(project, archive_id, projectDir, uploadDir, newRelativePath)
        end
      else 
        if File.ftype(fullName) == "file"
   
	  relName = my_join(relativePath,f)
 
          # delete any equivalent projectFile records
  	  project.project_archives.each do |c|
            old = ProjectFile.find(:first, :conditions => ['project_archive_id = ? and file_name = ?', c.id, relName])
            old.destroy if old
          end

          project_file = ProjectFile.new
          project_file.file_name = relName
          project_file.file_size = File.size(fullName)
          project_file.file_date = File.ctime(fullName)
          project_file.project_archive_id = archive_id 
          unless project_file.save
            flash[:error] = "System error saving project_file record for: #{f}."
            return false
          end
    
          parentDir = my_join(projectDir, relativePath)
          toName = my_join(parentDir, f)    
          # move file from temporary upload dir into parent dir
          File.rename(fullName,toName);
          FileUtils.chmod 0664, toName

        end
      end
    end
  end

  def prep_one_archive(project, filename, archive_no)

    # make sure parent paths exist
    projectDir = path_to_project_dir(project.id)

    # make a temporary upload directory to unpack and merge from
    uploadDir = projectDir+"/upload_#{archive_no}"
    clean_out_dir uploadDir
    Dir.mkdir(uploadDir,0775)

    cmd = makeUnarchiveCommand(project, uploadDir, filename)
    timeout = 3600
    exitCode = run_with_timeout(cmd, timeout)

    if exitCode != 0
      return false
    end
    return true

  end

  def reexpand_all_background(project_id)

    project = Project.find(project_id)
    new_status project, "re-expanding all"
    # make a hash of things to keep
    keepers = {}
    project.project_archives.each do |a|
      keepers[a.file_name] = "keep"
    end
    # keep other special files
    keepers["validate_error"] = "keep"
    keepers["load_error"] = "keep"
    keepers["upload_error"] = "keep"
    keepers["out"] = "keep"

    msg = ""
    # make sure parent paths exist
    projectDir = path_to_project_dir(project.id)

    # clean out directory
    Dir.entries(projectDir).each do 
      |f| 
      fullName = File.join(projectDir,f)
      unless keepers[f] or (f == ".") or (f == "..")
        cmd = "rm -fr #{fullName}"
        unless system(cmd)
          yell "System error cleaning up subdirectory: <br>command=[#{cmd}].<br>"  
          return false
        end
      end
    end

    project.project_archives.each do |a|
      a.project_files.each do |f|
        f.destroy
      end
    end

    found = false 
    project.project_archives.each do |a|
      n = a.archive_no-1
      c = project.archives_active[n..n]
      if c == "1"
        found = true
      end
    end

    if found
      project.project_archives.each do |a|
        n = a.archive_no-1
        c = project.archives_active[n..n]
        if c == "1"
          prep_one_archive project, a.file_name, a.archive_no
        end
      end
      reexpand_all_completion project
    else
      if project.project_archives.length == 0
        project.status = "new"
      else
        project.status = "uploaded"
      end
      new_status project, project.status
    end

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


  def sanitize_filename(file_name)
    # get only the filename, not the whole path (from IE)
    just_filename = File.basename(file_name) 
    # replace all non-alphanumeric, underscore or periods with underscore
    just_filename.gsub(/[^\w\.\_]/,'_') 
  end

  def my_join(path,name)
    if (path == "") 
      return name
    end
    if (name == "") 
      return path
    end
    return File.join(path,name)
  end


  # --------- PRIVATE ---------
private

  def reexpand_all_completion(project) 
    # after background expansion of all archives not marked as deleted

    project.project_archives.each do |a|
      n = a.archive_no-1
      c = project.archives_active[n..n]
      if c == "1"
        unless expand_archive(project, a)
          return
        end
      end
    end

  end

  def delete_completion(project_id)

    projectDir = path_to_project_dir(project_id)
    if File.exists?(projectDir)
      Dir.entries(projectDir).each { 
        |f| 
        unless (f == ".") or (f == "..")
          fullName = File.join(projectDir,f)
          cmd = "rm -fr #{fullName}"
          unless system(cmd)
            yell "System error cleaning out project subdirectory: <br>command=[#{cmd}].<br>" 
            # TODO maybe make better way to handle feedback to user?	
          end
        end
      }
      Dir.delete(projectDir)
    end
  end


  def path_to_project_dir(project_id)
    # the expand_path method resolves this relative path to full absolute path
    File.expand_path("#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}/#{project_id}")
  end


  def path_to_file(project_id, filename)
    # the expand_path method resolves this relative path to full absolute path
    path_to_project_dir(project_id)+"/#{filename}"
  end


  #TODO : turn projectTypes back into a yaml file in config dir

  # --- read project types from config file into hash -------
  def getProjectTypes
    #open("#{RAILS_ROOT}/config/projectTypes.yml") { |f| YAML.load(f.read) }
    ProjectType.find(:all, :conditions => ['display_order != 0'], :order => "display_order")
  end

  # --- read one project type from config file into hash -------
  def getProjectType(project)
    projectTypes = getProjectTypes
    projectTypes.each do |x|
      if x['id'] == project.project_type_id
        return x
      end
    end
  end


  def log_project_status(project)
    # add new projectArchive record
    project_status_log = ProjectStatusLog.new
    project_status_log.project_id = project.id
    project_status_log.status = project.status
    return saver(project_status_log)
  end

  def saver(record)
    unless record.save
      msg = "System error - #{record.table_name} record save failed.\n"
      record.errors.each_full { |x| msg += x + "\n" }
      yell msg
      return false
    end
    return true
  end
 
  def destroyer(record)
    unless record.destroy
      msg = "System error - #{record.table_name} record destroy failed.\n"
      record.errors.each_full { |x| msg += x + "\n" }
      yell msg
      return false
    end
    return true
  end

  def new_status(project, status)
    project.status = status
    if status.ends_with?(" requested")
      project.run_stat = "waiting"
    end
    unless saver project
      return false
    end
    return log_project_status(project)
  end
 
  def makeUnarchiveCommand(project, uploadDir, filename)
    # handle unzipping the archive
    pf = path_to_file(project.id, filename)
    if ["zip", "ZIP"].any? {|ext| filename.ends_with?("." + ext) }
      cmd = "unzip -o  #{pf} -d #{uploadDir} &> #{File.dirname(uploadDir)}/upload_error"   # .zip 
    else
      if ["gz", "GZ", "tgz", "TGZ"].any? {|ext| filename.ends_with?("." + ext) }
        cmd = "tar -xzf #{pf} -C #{uploadDir} &> #{File.dirname(uploadDir)}/upload_error"  # .gz .tgz gzip 
      else  
        cmd = "tar -xjf #{pf} -C #{uploadDir} &> #{File.dirname(uploadDir)}/upload_error"  # .bz2 bzip2
      end
    end
  end

  def clean_out_dir(path)
    if File.exists?(path)
      cmd = "rm -fr #{path}"
      unless system(cmd)
        flash[:error] = "System error cleaning out subdirectory: <br>command=[#{cmd}].<br>"  
        return false
      end
    end
    return true
  end

end
