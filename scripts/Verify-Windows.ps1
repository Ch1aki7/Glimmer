param(
    [string]$MSBuildPath = "",
    [switch]$SkipGenerate,
    [switch]$SkipBuild,
    [switch]$ForceTestFailure
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot

function Invoke-Checked {
    param(
        [string]$Description,
        [scriptblock]$Command
    )

    Write-Host "==> $Description"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

function Find-MSBuild {
    if ($MSBuildPath) {
        if (-not (Test-Path -LiteralPath $MSBuildPath -PathType Leaf)) {
            throw "MSBuild was not found at: $MSBuildPath"
        }
        return (Resolve-Path -LiteralPath $MSBuildPath).Path
    }

    $fromPath = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    $programFilesX86 = [Environment]::GetFolderPath('ProgramFilesX86')
    $vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
            -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
        if ($found) {
            return $found
        }
    }

    throw 'MSBuild.exe was not found. Install Visual Studio 2026 Desktop development with C++, or pass -MSBuildPath.'
}

function Assert-BuildSubmodulesInitialized {
    $requiredSubmodules = @(
        'Glimmer\vendor\spdlog',
        'Glimmer\vendor\GLFW',
        'Glimmer\vendor\imgui',
        'Glimmer\vendor\glm',
        'Glimmer\vendor\entt',
        'Glimmer\vendor\yaml-cpp',
        'Glimmer\vendor\ImGuizmo',
        'Glimmer\vendor\Vulkan-Headers',
        'Glimmer\vendor\SPIRV-Cross'
    )

    foreach ($relativePath in $requiredSubmodules) {
        $submodulePath = Join-Path $repoRoot $relativePath
        $submoduleGitMarker = Join-Path $submodulePath '.git'
        if (-not (Test-Path -LiteralPath $submodulePath -PathType Container) -or
            -not (Test-Path -LiteralPath $submoduleGitMarker)) {
            throw "Uninitialized build submodule: $relativePath. Run: git submodule update --init --recursive"
        }
    }
}

Push-Location $repoRoot
try {
    Assert-BuildSubmodulesInitialized

    if (-not $SkipGenerate) {
        $premake = Join-Path $repoRoot 'vendor\bin\premake\premake5.exe'
        if (-not (Test-Path -LiteralPath $premake -PathType Leaf)) {
            throw "Bundled Premake was not found at: $premake"
        }
        Invoke-Checked 'Generate Visual Studio 2026 projects' { & $premake vs2026 }
    }

    $resolvedMSBuild = Find-MSBuild
    if (-not $SkipBuild) {
        $solution = if (Test-Path -LiteralPath 'GlimmerEngine.slnx') {
            'GlimmerEngine.slnx'
        } else {
            'GlimmerEngine.sln'
        }
        Invoke-Checked "Build $solution (Debug | x64)" {
            & $resolvedMSBuild $solution '/t:Build' `
                '/p:Configuration=Debug' '/p:Platform=x64' '/m:4' '/v:minimal'
        }
    }

    $testExecutable = Join-Path $repoRoot `
        'bin\Debug-windows-x86_64\GlimmerRegressionTests\GlimmerRegressionTests.exe'
    if (-not (Test-Path -LiteralPath $testExecutable -PathType Leaf)) {
        throw "Regression test executable was not found at: $testExecutable"
    }

    if ($ForceTestFailure) {
        Invoke-Checked 'Run intentional regression failure' {
            & $testExecutable '--force-failure'
        }
    } else {
        Invoke-Checked 'Run headless regression tests' { & $testExecutable }
    }
    Write-Host '==> Glimmer Windows verification passed.' -ForegroundColor Green
}
finally {
    Pop-Location
}
