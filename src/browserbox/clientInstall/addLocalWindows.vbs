'VBS script to add genome.ucsc.local to hosts file for UCSC Genome browserbox '

Set WshShell = WScript.CreateObject("WScript.Shell")

' if not called with parameter, run this script again with admin rights '
If WScript.Arguments.length = 0 Then
Set ObjShell = CreateObject("Shell.Application")
ObjShell.ShellExecute "wscript.exe", """" & _
WScript.ScriptFullName & """" &_
" RunAsAdministrator", , "runas", 1
WScript.quit
End if


' find and open to hosts file '
Const ForReading = 1, ForWriting = 2, ForAppending = 8, ReadOnly = 1
Set fso = CreateObject("Scripting.FileSystemObject")
Set WshShell=CreateObject("WScript.Shell")
WinDir =WshShell.ExpandEnvironmentStrings("%WinDir%")

HostsFile = WinDir & "\System32\Drivers\etc\Hosts"

Set objFSO = CreateObject("Scripting.FileSystemObject")
Set objFile = objFSO.OpenTextFile(HostsFile, ForReading)

' check if the string is already present '
Do Until objFile.AtEndOfStream
If InStr (objFile.ReadLine, "genome.ucsc.local") <> 0 Then
MsgBox("genome.ucsc.local already present in hosts file, no changes made")
WScript.Quit
End If
i = i + 1
Loop
objFile.Close

' remove read only attribute from file '
Set objFSO = CreateObject("Scripting.FileSystemObject")
Set objFile = objFSO.GetFile(HostsFile)
If objFile.Attributes AND ReadOnly Then
objFile.Attributes = objFile.Attributes XOR ReadOnly
End If

' append to file '
Set filetxt = fso.OpenTextFile(HostsFile, ForAppending, True)
filetxt.WriteLine(vbNewLine & "127.0.0.1 genome.ucsc.local")
filetxt.Close

MsgBox("Added entry for genome.ucsc.local to host names file")
WScript.quit
