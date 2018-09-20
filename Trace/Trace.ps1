#requires -Version 4.0

<#
 .SYNOPSIS
 Captures ETW traces.

 .PARAMETER Output
 Output path. A temporary folder is created by default.

 .PARAMETER Help
 Shows the detailed help information.

 .PARAMETER List
 Displays the list of available scenarios.

 .PARAMETER Detailed
 Shows additional information about the scenarios available.

 .EXAMPLE
 .\Trace.ps1 -List
 Shows information about the supported scenarios.

 .EXAMPLE
 .\Trace.ps1 -List -Detailed
 Shows information about the supported scenarios, including providers used.
#>

[CmdletBinding(DefaultParameterSetName = "Common")]
param(
    [Parameter(ParameterSetName = "Common", ValueFromPipeline = $true)]
    [Alias("o")]
    [String]
    $Output,

    [Parameter(ParameterSetName = "Help")]
    [Alias("h", "?")]
    [Switch]
    $Help,

    [Parameter(ParameterSetName = "List")]
    [Switch]
    $Detailed
)

#--------------------------------------------------------------------
# Variables
#--------------------------------------------------------------------

# Scenario to be used, when the "Default" scenario is chosen.
$Scenario = New-Object System.Collections.ArrayList
$Scenario.Add("Multimedia") > $null

$Modules = @(

    # Needed to access caller variables ($Verbose, $Debug, and etc.) from the included modules.
    "$PSScriptRoot\lib\Get-CallerPreference.psm1"

    # Helper functions.
    "$PSScriptRoot\lib\Types.psm1"
    "$PSScriptRoot\lib\Utils.psm1"
)

# Contains information about the local and target environments.
$EnvironmentInfo = $null

# Collection of scenarios to be executed.
$ScenariosData = @{}

# List of tracing script paths.
$StartScripts          = New-Object System.Collections.ArrayList
$StopScripts           = New-Object System.Collections.ArrayList
$PostProcessingScripts = New-Object System.Collections.ArrayList

# List of outstanding background jobs.
$BackgroundJobs        = New-Object System.Collections.ArrayList

#--------------------------------------------------------------------
# Functions
#--------------------------------------------------------------------

<#
 .SYNOPSIS
 Script entry point.
#>
function Main {
    [CmdletBinding()]
    [OutputType([void])]
    param()

    begin {
        Import-Module -Name $Modules -Force -NoClobber:$false -DisableNameChecking -ErrorAction Stop -Function * -Alias * -Cmdlet * -Variable *

        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        if ($Help) {
            Get-Help $MyInvocation.PSCommandPath -Detailed
            return
        }

        . $PSScriptRoot\lib\TraceFn.ps1

        # Convert the input arguments to the proper strongly-typed values.
        $script:OperationModeType  = Read-TraceOperationMode      "NoPostProcessing"
        $script:TargetType         = Read-TraceTargetType         $null
        $script:ToolsetType        = Read-TraceToolsetType        "WPR"
        $script:PostProcessingType = Read-TracePostProcessingType "None"

        #
        # Make sure the script is running elevated.
        #

        if (!(Test-DeviceConnected) -and !(Test-InAdminRole)) {
            Write-Error "Please, run the current script from the elevated command prompt"
            return
        }
        #
        # Main loop.
        #

        $tracingStarted = $false

        try {
            Write-Host "Looking for the logging scenarios..."
            #$script:ScenariosData = Get-Scenario $Scenario

            Write-Host "Gathering system information..."
            Get-EnvironmentInformation

            Write-Host "Preparing local system..."
            Prepare-Local

            # If the user wants us to generate scripts, but not to run them.
            Write-Host "Preparing target system..."
            Prepare-Target

            Write-Host "Saving target system details..."
            Save-TargetDetails

            Write-Host "Creating tracing scripts..."
            Create-Scripts

            Write-Host "Starting tracing..."
            $tracingStarted = $true
            Start-Tracing

            # Wait for user input, or for background tasks to finish.
            Wait-ForStop

            Write-Host "Stopping tracing and merging results..."
            Stop-Tracing -DownloadFiles
            $tracingStarted = $false

            Write-Host "Waiting for the background jobs to complete..."
            Wait-ForBackgroundJobs

            Write-Host "Output: $($EnvironmentInfo.TracePathLocal)"

        } finally {
            # Open the output directory.
            if (Test-Path $EnvironmentInfo.TracePathLocal) {
                Start-Process ($EnvironmentInfo.TracePathLocal) > $null
            }

            if ($tracingStarted) {
                # No need to download files in case we were interrupted.
                Stop-Tracing -DownloadFiles:$false
            }
        }
        Write-Host "Done"
    }
}

<#
 .SYNOPSIS
 Populates the $EnvironmentInfo object.
#>
function Get-EnvironmentInformation {
    [CmdletBinding()]
    [OutputType([void])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[Get-EnvironmentInformation] Collecting environment information"

        $systemArchitecture = Get-LocalSystemArchitecture

        # Figure out if xperf can be found in the PATH.
        cmd /c where /Q xperf.exe > $null
        if ($LASTEXITCODE -eq 0) {
            $localXPerf = cmd /c where xperf.exe
        } else {
            $localXPerf = "C:\Windows\System32\XPerf.exe"
        }

        $script:EnvironmentInfo = New-Object Tracing.EnvironmentInfo
        $EnvironmentInfo.ScriptRootPath          = $PSScriptRoot
        $EnvironmentInfo.TargetTemp              = if ($TargetType -eq [Tracing.TargetType]::TShell) { "C:\Data\Test\Tracing" } elseif ($TargetType -eq [Tracing.TargetType]::XBox) { "D:\Tmp\Tracing" } else { $env:TEMP }
        $EnvironmentInfo.SymLocalPath            = "$($env:SystemDrive)\Symbols.pri"
        $EnvironmentInfo.TraceEtlBase            = "Trace_$($env:USERNAME)_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
        $EnvironmentInfo.TracePathTarget         = "$($EnvironmentInfo.TargetTemp)\$($EnvironmentInfo.TraceEtlBase)"
        $EnvironmentInfo.TracePathLocal          = if ([String]::IsNullOrEmpty($Output)) { "$($env:TEMP)\$($EnvironmentInfo.TraceEtlBase)" } else { $Output }
        $EnvironmentInfo.TraceScriptsPathLocal   = "$($EnvironmentInfo.TracePathLocal)\Scripts"
        $EnvironmentInfo.TraceScriptsPathTarget  = "$($EnvironmentInfo.TracePathTarget)\Scripts"
        

        Write-Verbose "[Get-EnvironmentInformation] Data:"
        Write-Verbose "  ScriptRootPath          : $($EnvironmentInfo.ScriptRootPath)"
        Write-Verbose "  TargetTemp              : $($EnvironmentInfo.TargetTemp)"
        Write-Verbose "  SymLocalPath            : $($EnvironmentInfo.SymLocalPath)"
        Write-Verbose "  TraceEtlBase            : $($EnvironmentInfo.TraceEtlBase)"
        Write-Verbose "  TracePathTarget         : $($EnvironmentInfo.TracePathTarget)"
        Write-Verbose "  TracePathLocal          : $($EnvironmentInfo.TracePathLocal)"
        Write-Verbose "  TraceScriptsPathLocal   : $($EnvironmentInfo.TraceScriptsPathLocal)"
        Write-Verbose "  TraceScriptsPathTarget  : $($EnvironmentInfo.TraceScriptsPathTarget)"

        Write-Verbose "[Get-EnvironmentInformation] Done"
    }
}

<#
 .SYNOPSIS
 Stores system information in the output folder.
#>
function Save-TargetDetails {
    [CmdletBinding()]
    [OutputType([void])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState

        [void]$BackgroundJobs.Clear()
    }

    process {

        try{
            $buildInfo = Get-BuildInfo -TargetType $TargetType
            $buildInfoFilePath = "$($EnvironmentInfo.TracePathTarget)\BuildInfo.log"

            $buildInfoStr = New-Object System.Text.StringBuilder
            [void]$buildInfoStr.AppendLine("Version=$($buildInfo.Version),Arch=$($buildInfo.Flavor),Branch=$($buildInfo.Branch)")
            $buildInfoStr.ToString() | Set-Content $buildInfoFilePath -Encoding Ascii

        } catch {
                Write-Verbose "[Save-TargetDetails] Error while getting buildInfo: $_"
        }

        if ($TargetType -eq [Tracing.TargetType]::Local) {
            
            Write-Verbose "[Save-TargetDetails] Collecting machine information and crash reports"

            #
            # Collect information about the machine.
            #
            Gather-DxDiag
            Gather-SetupAPILog
            Gather-PnpUtil

            # Save buildInfo
        }
    }
}

#--------------------------------------------------------------------
# Commands
#--------------------------------------------------------------------

# Set the default error action to 'Stop'.
$ErrorActionPreference = "Stop"
$WarningPreference     = "Continue"

Main
