[CmdletBinding()]
param(
    [string]$BuildDir = "build-qt15",
    [string]$Configuration = "Release",
    [string]$DistDir = "dist",
    [string]$PackageName = "",
    [switch]$SkipBuild,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

function Invoke-CheckedCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

function Get-RepositoryRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Get-HFTextVersion {
    param([string]$RepositoryRoot)

    $headerPath = Join-Path $RepositoryRoot "core\include\hftext_version.h"
    $header = Get-Content -LiteralPath $headerPath -Raw
    if ($header -match 'kVersion\s*=\s*"([^"]+)"') {
        return $Matches[1]
    }
    return "dev"
}

function Assert-PathUnderRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $fullRoot = [System.IO.Path]::GetFullPath($Root)
    if (-not $fullPath.StartsWith($fullRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to operate outside ${Root}: ${Path}"
    }
}

function Find-BuildArtifact {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildRoot,
        [Parameter(Mandatory = $true)]
        [string[]]$RelativeCandidates
    )

    foreach ($relative in $RelativeCandidates) {
        $candidate = Join-Path $BuildRoot $relative
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "Build artifact not found. Tried: $($RelativeCandidates -join ', ')"
}

function Copy-MsvcRuntime {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PackageDir
    )

    $vsRoot = "C:\Program Files (x86)\Microsoft Visual Studio"
    if (-not (Test-Path -LiteralPath $vsRoot)) {
        Write-Warning "Visual Studio root not found; MSVC runtime DLLs were not copied."
        return
    }

    $crtDirs = Get-ChildItem -LiteralPath $vsRoot -Directory -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\VC\\Redist\\MSVC\\[^\\]+\\x64\\Microsoft\.VC.*\.CRT$' } |
        Sort-Object FullName -Descending

    $crtDir = $crtDirs | Select-Object -First 1
    if ($null -eq $crtDir) {
        Write-Warning "MSVC x64 CRT redist directory not found; runtime DLLs were not copied."
        return
    }

    Copy-Item -Path (Join-Path $crtDir.FullName "*.dll") -Destination $PackageDir -Force
}

$repoRoot = Get-RepositoryRoot
Push-Location $repoRoot
try {
    if (-not $SkipBuild) {
        Invoke-CheckedCommand cmake "--build" $BuildDir "--config" $Configuration
    }

    if (-not $SkipTests) {
        Invoke-CheckedCommand ctest "--test-dir" $BuildDir "-C" $Configuration "--output-on-failure"
    }

    $version = Get-HFTextVersion -RepositoryRoot $repoRoot
    if ([string]::IsNullOrWhiteSpace($PackageName)) {
        $PackageName = "HFText-win64-release-${version}-$(Get-Date -Format 'yyyyMMdd-HHmm')"
    }

    $buildRoot = (Resolve-Path -LiteralPath $BuildDir).Path
    $distRoot = Join-Path $repoRoot $DistDir
    New-Item -ItemType Directory -Path $distRoot -Force | Out-Null

    $packageDir = Join-Path $distRoot $PackageName
    $zipPath = "${packageDir}.zip"
    Assert-PathUnderRoot -Path $packageDir -Root $distRoot
    Assert-PathUnderRoot -Path $zipPath -Root $distRoot

    if (Test-Path -LiteralPath $packageDir) {
        Remove-Item -LiteralPath $packageDir -Recurse -Force
    }
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    New-Item -ItemType Directory -Path $packageDir -Force | Out-Null

    $pcExe = Find-BuildArtifact -BuildRoot $buildRoot -RelativeCandidates @(
        "pc-app\$Configuration\hftext_pc.exe",
        "pc-app\hftext_pc.exe"
    )
    Copy-Item -LiteralPath $pcExe -Destination $packageDir -Force

    $windeployqt = (Get-Command windeployqt -ErrorAction SilentlyContinue)
    if ($null -eq $windeployqt) {
        throw "windeployqt was not found in PATH. Add the Qt bin directory to PATH before packaging."
    }
    Invoke-CheckedCommand $windeployqt.Source "--release" "--compiler-runtime" "--dir" $packageDir (Join-Path $packageDir "hftext_pc.exe")
    Copy-MsvcRuntime -PackageDir $packageDir

    foreach ($tool in @("hftext_tx_wav.exe", "hftext_rx_wav.exe", "hftext_stream_wav.exe")) {
        $artifact = Find-BuildArtifact -BuildRoot $buildRoot -RelativeCandidates @(
            "core\$Configuration\$tool",
            "core\$tool"
        )
        Copy-Item -LiteralPath $artifact -Destination $packageDir -Force
    }

    Copy-Item -LiteralPath (Join-Path $repoRoot "README.md") -Destination $packageDir -Force
    Copy-Item -LiteralPath (Join-Path $repoRoot "AGENTS.md") -Destination $packageDir -Force

    New-Item -ItemType Directory -Path (Join-Path $packageDir "docs") -Force | Out-Null
    Copy-Item -Path (Join-Path $repoRoot "docs\*.md") -Destination (Join-Path $packageDir "docs") -Force

    New-Item -ItemType Directory -Path (Join-Path $packageDir "core") -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot "core\README.md") -Destination (Join-Path $packageDir "core\README.md") -Force

    New-Item -ItemType Directory -Path (Join-Path $packageDir "python-sim") -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot "python-sim\README.md") -Destination (Join-Path $packageDir "python-sim\README.md") -Force

    $gitSha = "unknown"
    $gitDirty = "unknown"
    if (Get-Command git -ErrorAction SilentlyContinue) {
        $gitSha = (& git rev-parse --short HEAD 2>$null)
        $gitDirty = if ((& git status --porcelain 2>$null)) { "yes" } else { "no" }
    }

    $manifest = @(
        "HFText release package",
        "Version: $version",
        "Configuration: $Configuration",
        "Built at: $(Get-Date -Format o)",
        "Git commit: $gitSha",
        "Git dirty: $gitDirty",
        "Package: $PackageName"
    )
    $manifest | Set-Content -LiteralPath (Join-Path $packageDir "PACKAGE.txt") -Encoding UTF8

    try {
        Compress-Archive -Path $packageDir -DestinationPath $zipPath -Force
    } catch {
        if (Test-Path -LiteralPath $zipPath) {
            Remove-Item -LiteralPath $zipPath -Force
        }
        Invoke-CheckedCommand tar "-a" "-c" "-f" $zipPath "-C" $distRoot $PackageName
    }

    Get-Item -LiteralPath $zipPath | Select-Object FullName, Length, LastWriteTime
} finally {
    Pop-Location
}
