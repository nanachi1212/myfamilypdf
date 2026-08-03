[CmdletBinding()]
param(
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildRoot = [IO.Path]::GetFullPath(
    (Join-Path $repositoryRoot 'build')
).TrimEnd('\') + '\'

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot `
        'build\acrobat-document-edit-interop'
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $OutputDirectory.StartsWith(
        $buildRoot,
        [StringComparison]::OrdinalIgnoreCase
    )) {
    throw (
        'Acrobat smoke output must remain under the repository build ' +
        "directory: $OutputDirectory"
    )
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$acrobat = 'C:\Program Files\Adobe\Acrobat DC\Acrobat\Acrobat.exe'
if (-not (Test-Path -LiteralPath $acrobat -PathType Leaf)) {
    throw (
        'Adobe Acrobat was not found; this optional cross-product smoke ' +
        'test requires Acrobat.'
    )
}
if (Get-Process -Name Acrobat -ErrorAction SilentlyContinue) {
    throw (
        'Adobe Acrobat is already running. Close it before this isolated ' +
        'smoke test.'
    )
}

function Stop-IsolatedAcrobat {
    $processes = @(Get-Process -Name Acrobat -ErrorAction SilentlyContinue)
    if ($processes.Count -eq 0) {
        return
    }
    $processes | Wait-Process -Timeout 5 -ErrorAction SilentlyContinue
    Get-Process -Name Acrobat -ErrorAction SilentlyContinue |
        Stop-Process -Force
}

$venvPython = Join-Path (
    Split-Path $repositoryRoot -Parent
) 'FamilyPDF-tools\office-export-venv\Scripts\python.exe'
& (Join-Path $repositoryRoot `
    'scripts\office\install-office-export-toolchain.ps1')
if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
    throw "Office verification Python was not prepared: $venvPython"
}

$fixtures = @(
    [pscustomobject]@{
        name = 'decorations'
        input = Join-Path $repositoryRoot `
            'dist\qa\document-edit-interop.pdf'
    },
    [pscustomobject]@{
        name = 'geometry'
        input = Join-Path $repositoryRoot `
            'dist\qa\page-geometry-interop.pdf'
    }
)
foreach ($fixture in $fixtures) {
    if (-not (Test-Path -LiteralPath $fixture.input -PathType Leaf)) {
        throw "Document-edit fixture was not found: $($fixture.input)"
    }
    $fixture | Add-Member -NotePropertyName output -NotePropertyValue (
        Join-Path $OutputDirectory "$($fixture.name)-acrobat-saved.pdf"
    )
}

$driver = Join-Path $PSScriptRoot `
    'acrobat-document-edit-interop.vbs'
foreach ($fixture in $fixtures) {
    $driverSucceeded = $false
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        Remove-Item -LiteralPath $fixture.output -Force `
            -ErrorAction SilentlyContinue
        $savedErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            $driverOutput = @(
                & "$env:SystemRoot\System32\cscript.exe" //nologo `
                    $driver $fixture.input $fixture.output 2>&1
            )
            $driverExitCode = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $savedErrorActionPreference
        }
        $driverOutput | Write-Host
        if (
            $driverExitCode -eq 0 -and
            (Test-Path -LiteralPath $fixture.output -PathType Leaf)
        ) {
            $driverSucceeded = $true
            Stop-IsolatedAcrobat
            Start-Sleep -Seconds 3
            break
        }

        # The precondition rejects pre-existing Acrobat sessions, so these
        # processes belong to the failed isolated verification attempt.
        Stop-IsolatedAcrobat
        if ($attempt -lt 3) {
            Write-Warning (
                "Acrobat COM attempt $attempt failed with exit code " +
                "$driverExitCode; retrying."
            )
            Start-Sleep -Seconds 3
        }
    }
    if (-not $driverSucceeded) {
        throw (
            "Adobe Acrobat could not round-trip $($fixture.name) after " +
            'three attempts.'
        )
    }
}

$decorationsInput = $fixtures[0].input
$decorationsOutput = $fixtures[0].output
$geometryInput = $fixtures[1].input
$geometryOutput = $fixtures[1].output
& $venvPython -c @"
from hashlib import sha256
from pathlib import Path

import pypdfium2 as pdfium
from pypdf import PdfReader

decorations_input = Path(r'$decorationsInput')
decorations_output = Path(r'$decorationsOutput')
geometry_input = Path(r'$geometryInput')
geometry_output = Path(r'$geometryOutput')


def render_fingerprints(path):
    document = pdfium.PdfDocument(str(path))
    fingerprints = []
    for page in document:
        image = page.render(scale=2.0).to_pil().convert('RGB')
        digest = sha256()
        digest.update(f'{image.width}x{image.height}:RGB'.encode('ascii'))
        digest.update(image.tobytes())
        fingerprints.append(digest.hexdigest())
    return fingerprints


before_decorations = PdfReader(str(decorations_input))
after_decorations = PdfReader(str(decorations_output))
assert len(before_decorations.pages) == 3
assert len(after_decorations.pages) == 3
assert render_fingerprints(decorations_input) == render_fingerprints(
    decorations_output
), 'Watermark/background rendering changed after Acrobat save'

before_geometry = PdfReader(str(geometry_input))
after_geometry = PdfReader(str(geometry_output))
assert len(before_geometry.pages) == 2
assert len(after_geometry.pages) == 2
for before_page, after_page in zip(
    before_geometry.pages, after_geometry.pages
):
    assert list(before_page.mediabox) == list(after_page.mediabox)
    assert list(before_page.cropbox) == list(after_page.cropbox)
    assert int(before_page.get('/Rotate', 0)) == int(
        after_page.get('/Rotate', 0)
    )
assert render_fingerprints(geometry_input) == render_fingerprints(
    geometry_output
), 'Page geometry rendering changed after Acrobat save'
"@
if ($LASTEXITCODE -ne 0) {
    throw (
        'Independent parser/raster verification of Acrobat outputs failed.'
    )
}

$results = foreach ($fixture in $fixtures) {
    $inputHash = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $fixture.input
    ).Hash
    $outputHash = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $fixture.output
    ).Hash
    if ($inputHash -eq $outputHash) {
        throw (
            "Acrobat output for $($fixture.name) is byte-identical to " +
            'the input; a new file was not written.'
        )
    }
    [ordered]@{
        name = $fixture.name
        input = $fixture.input
        input_sha256 = $inputHash
        output = $fixture.output
        output_sha256 = $outputHash
        structure_preserved = $true
        rendering_preserved = $true
    }
}

$summary = [ordered]@{
    recorded_at = [DateTimeOffset]::Now.ToString('o')
    acrobat = $acrobat
    independent_parser = 'pypdf'
    independent_renderer = 'pypdfium2'
    documents = @($results)
}
$summaryPath = Join-Path $OutputDirectory 'summary.json'
$summary | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host (
    'Adobe Acrobat document-edit round-trip passed: ' +
    $summaryPath
)
