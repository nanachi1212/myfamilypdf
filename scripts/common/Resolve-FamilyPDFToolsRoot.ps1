function Resolve-FamilyPDFToolsRoot {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot,
        [string]$ExplicitRoot = ''
    )

    $root = if (-not [string]::IsNullOrWhiteSpace($ExplicitRoot)) {
        $ExplicitRoot
    }
    elseif (-not [string]::IsNullOrWhiteSpace($env:FAMILYPDF_TOOLS_ROOT)) {
        $env:FAMILYPDF_TOOLS_ROOT
    }
    else {
        Join-Path (Split-Path $RepositoryRoot -Parent) 'FamilyPDF-tools'
    }
    return [IO.Path]::GetFullPath($root)
}
