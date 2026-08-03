Option Explicit

Dim inputPath
Dim outputPath
Dim app
Dim avDoc
Dim pdDoc
Dim page
Dim pageCount
Dim pageIndex
Dim acquireAttempt
Dim saved
Dim exitCode

If WScript.Arguments.Count <> 2 Then
    WScript.Echo "Usage: cscript acrobat-document-edit-interop.vbs <input-pdf> <output-pdf>"
    WScript.Quit 2
End If

inputPath = WScript.Arguments(0)
outputPath = WScript.Arguments(1)
Set app = CreateObject("AcroExch.App")
Set avDoc = CreateObject("AcroExch.AVDoc")

If Not avDoc.Open(inputPath, "") Then
    app.Exit
    WScript.Echo "Acrobat could not open the PDF."
    WScript.Quit 3
End If

Set pdDoc = avDoc.GetPDDoc
pageCount = pdDoc.GetNumPages
If pageCount < 1 Then
    avDoc.Close True
    app.Exit
    WScript.Echo "Acrobat reported an empty document."
    WScript.Quit 4
End If

' Acquire every page before saving. This forces page-tree parsing instead of
' only proving that Acrobat can open the document container.
For pageIndex = 0 To pageCount - 1
    Set page = Nothing
    For acquireAttempt = 1 To 20
        Set page = pdDoc.AcquirePage(pageIndex)
        If Not page Is Nothing Then
            Exit For
        End If
        WScript.Sleep 500
    Next
    If page Is Nothing Then
        avDoc.Close True
        app.Exit
        WScript.Echo "Acrobat could not acquire page " & pageIndex & "."
        WScript.Quit 5
    End If
    Set page = Nothing
Next

saved = pdDoc.Save(1, outputPath)
avDoc.Close True
app.Exit

exitCode = 0
If Not saved Then
    WScript.Echo "Acrobat could not save the PDF."
    exitCode = 6
Else
    WScript.Echo "Acrobat document-edit interoperability save passed."
End If

WScript.Quit exitCode
