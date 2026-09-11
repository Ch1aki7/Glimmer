param(
    [string]$MSBuildPath = "",
    [ValidateRange(1, 32)]
    [int]$BuildJobs = 1,
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

function Invoke-MSBuildWithNormalizedEnvironment {
    param(
        [string]$Executable,
        [string[]]$Arguments
    )

    # Some launchers expose both PATH and Path. MSBuild treats that raw process
    # environment block as invalid (MSB6001), even though PowerShell normally
    # hides the duplicate. Build a child environment with one canonical PATH.
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.WorkingDirectory = $repoRoot
    $startInfo.UseShellExecute = $false
    $startInfo.Environment.Clear()
    foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) {
        if ($entry.Key -ine 'Path') {
            $startInfo.Environment[$entry.Key] = [string]$entry.Value
        }
    }
    $startInfo.Environment['PATH'] = $env:PATH

    if ($startInfo.PSObject.Properties.Name -contains 'ArgumentList') {
        foreach ($argument in $Arguments) {
            [void]$startInfo.ArgumentList.Add($argument)
        }
    } else {
        $escapedArguments = foreach ($argument in $Arguments) {
            if ($argument -match '[\s"]') {
                '"' + $argument.Replace('"', '\"') + '"'
            } else {
                $argument
            }
        }
        $startInfo.Arguments = $escapedArguments -join ' '
    }
    $process = [System.Diagnostics.Process]::Start($startInfo)
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
        throw "MSBuild failed with exit code $($process.ExitCode)."
    }
}

function Assert-BuildInputsAvailable {
    $requiredSubmodules = @(
        @{ Path = 'Glimmer\vendor\spdlog'; Sentinel = 'include\spdlog\spdlog.h' },
        @{ Path = 'Glimmer\vendor\GLFW'; Sentinel = 'include\GLFW\glfw3.h' },
        @{ Path = 'Glimmer\vendor\imgui'; Sentinel = 'imgui.cpp' },
        @{ Path = 'Glimmer\vendor\glm'; Sentinel = 'glm\glm.hpp' },
        @{ Path = 'Glimmer\vendor\assimp'; Sentinel = 'CMakeLists.txt' },
        @{ Path = 'Glimmer\vendor\entt'; Sentinel = 'src\entt\entt.hpp' },
        @{ Path = 'Glimmer\vendor\yaml-cpp'; Sentinel = 'src\node.cpp' },
        @{ Path = 'Glimmer\vendor\ImGuizmo'; Sentinel = 'src\ImGuizmo.cpp' },
        @{ Path = 'Glimmer\vendor\Vulkan-Headers'; Sentinel = 'include\vulkan\vulkan.h' },
        @{ Path = 'Glimmer\vendor\SPIRV-Cross'; Sentinel = 'spirv_cross.cpp' }
    )

    foreach ($dependency in $requiredSubmodules) {
        $submodulePath = Join-Path $repoRoot $dependency.Path
        $submoduleGitMarker = Join-Path $submodulePath '.git'
        if (-not (Test-Path -LiteralPath $submodulePath -PathType Container) -or
            -not (Test-Path -LiteralPath $submoduleGitMarker)) {
            throw "Uninitialized build submodule: $($dependency.Path). Run: git submodule update --init --recursive"
        }
        $sentinelPath = Join-Path $submodulePath $dependency.Sentinel
        if (-not (Test-Path -LiteralPath $sentinelPath -PathType Leaf)) {
            throw "Incomplete build submodule: $($dependency.Path) is missing $($dependency.Sentinel). Run: git submodule update --init --recursive --force"
        }
    }

    $repositoryOwnedInputs = @(
        'premake5.lua',
        'scripts\premake\Dependencies.lua',
        'Glimmer\vendor\Glad\src\glad.c',
        'vendor\bin\premake\premake5.exe'
    )
    foreach ($relativePath in $repositoryOwnedInputs) {
        $inputPath = Join-Path $repoRoot $relativePath
        if (-not (Test-Path -LiteralPath $inputPath -PathType Leaf)) {
            throw "Missing repository build input: $relativePath. Restore tracked files before generating projects."
        }
    }
}

Push-Location $repoRoot
try {
    Assert-BuildInputsAvailable

    if (-not $SkipGenerate) {
        $premake = Join-Path $repoRoot 'vendor\bin\premake\premake5.exe'
        if (-not (Test-Path -LiteralPath $premake -PathType Leaf)) {
            throw "Bundled Premake was not found at: $premake"
        }
        Invoke-Checked 'Generate Visual Studio 2026 projects' { & $premake vs2026 }
    }

    $resolvedMSBuild = Find-MSBuild
    if (-not $SkipBuild) {
        $ensureAssimp = Join-Path $repoRoot 'scripts\Win-EnsureAssimp-vs2026.bat'
        Invoke-Checked 'Ensure Assimp static dependency (Debug)' {
            & $ensureAssimp Debug
        }

        $solution = if (Test-Path -LiteralPath 'GlimmerEngine.slnx') {
            'GlimmerEngine.slnx'
        } else {
            'GlimmerEngine.sln'
        }
        Write-Host "==> Build $solution (Debug | x64)"
        Invoke-MSBuildWithNormalizedEnvironment $resolvedMSBuild @(
            $solution,
            '/t:Build',
            '/p:Configuration=Debug',
            '/p:Platform=x64',
            "/m:$BuildJobs",
            '/v:minimal'
        )
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
