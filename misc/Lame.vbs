' lame.vbs WindowsScript wrapper v0.2, 10/2000
'
' *Purpose*
' Use this WindowsScript to encode WAVs using drag&drop:
' 0. make sure you have the latest windows script host on your system
'    (enter 'cscript' in a DOS-Box, you should have version >=5.1)
' 1. adjust the path settings below to fit your needs
' 2. put this file somewhere on the desktop
' 3. drag one or more wav-files on the icon and watch them being lamed.
'
'
' feel free to extend this with a better user-interface ;-)
'
' Ralf Kempkens, ralf.kempkens@epost.de


' *** change path to your needs ***
   
    lame = "D:\Audio\Lame\" & "lame.exe"

' *** change default options to your needs ***
   
    opts = "--preset cd"

Dim wsh, args, infile, fs

' get input files
Set wsh = WScript.CreateObject("WScript.Shell")
Set args = WScript.Arguments
If args.Count = 0 Then
  MsgBox "Please use drag & drop to specify input files."
  WScript.Quit
End If

' check path
Set fs = CreateObject("Scripting.FileSystemObject")
If Not fs.FileExists(lame) Then
  wsh.Popup "Could not find LAME!" & Chr(13) & "(looked for '" & lame & "')", , "Error", 16
  WScript.Quit
End If

For i = 0 To args.Count-1
  infile = args(i)
  ' check input file
  If Not fs.FileExists(infile) Then
    wsh.Popup "Error opening input-file" & Chr(13) & "'" & infile & "'", , "Error", 16
  Else
    ' run lame
    ret = wsh.Run(lame & Chr(32) & opts & Chr(32) & Chr(34) & infile & Chr(34), 1, True)

    ' diagnostics
    Select Case ret
    Case (-1)
      MsgBox "LAME aborted by user!"
    Case (1)
      MsgBox "Error returned by LAME!" & Chr(13) & "(Check LAME options and input file formats.)"
    Case Else
      Rem ignore
    End Select
  End If
Next
'eof
