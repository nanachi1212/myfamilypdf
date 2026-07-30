Option Explicit

Dim inputPath
Dim outputPath
Dim app
Dim avDoc
Dim pdDoc
Dim js
Dim fieldCount
Dim index
Dim fieldName
Dim field
Dim textChanged
Dim checkChanged
Dim saved
Dim exitCode

If WScript.Arguments.Count <> 2 Then
    WScript.Echo "Usage: cscript acrobat-form-interop.vbs <input-pdf> <output-pdf>"
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
Set js = pdDoc.GetJSObject
fieldCount = js.numFields
textChanged = False
checkChanged = False

For index = 0 To fieldCount - 1
    fieldName = js.getNthFieldName(index)
    Set field = js.getField(fieldName)
    WScript.Echo "FIELD[" & index & "] type=" & field.type
    If field.type = "text" Then
        field.value = "AdobeInterop2026"
        textChanged = True
    ElseIf field.type = "checkbox" Then
        field.value = "Off"
        checkChanged = True
    End If
Next

saved = False
If textChanged And checkChanged Then
    saved = pdDoc.Save(1, outputPath)
End If

avDoc.Close True
app.Exit

exitCode = 0
If Not textChanged Then
    WScript.Echo "No text field was changed."
    exitCode = 4
ElseIf Not checkChanged Then
    WScript.Echo "No checkbox was changed."
    exitCode = 5
ElseIf Not saved Then
    WScript.Echo "Acrobat could not save the PDF."
    exitCode = 6
Else
    WScript.Echo "Acrobat form interoperability write passed."
End If

WScript.Quit exitCode
