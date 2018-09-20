#requires -Version 4.0
#--------------------------------------------------------------------
# Functions
#--------------------------------------------------------------------
<#
 .SYNOPSIS
 Waits for user input or for the UI wait task to complete before returning.
 UI wait task is just a wrapper around 'AutoIt3::WinWait' function:
 https://www.autoitscript.com/autoit3/docs/functions/WinWait.htm
#>
function Wait-ForStop {
    [CmdletBinding()]
    [OutputType([void])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState

        $ise     = Test-InIse
        $waitJob = $null
    }

    process {
        try {
            # Start a new background job, if user wants us to wait for a text.
            if ($WinWait.Count -gt 0) {
                $waitJob = Start-Job -ArgumentList @($WinWait, "$($EnvironmentInfo.ScriptRootPath)\bin\AutoIt\AutoItX3.psd1") -ScriptBlock {
                    param(
                        [Parameter(Mandatory = $true)]
                        [Object[]]
                        $winWait,

                        [Parameter(Mandatory = $true)]
                        [string]
                        $autoItModule
                    )

                    # Import the AutoIt3 module, so we'll be able to wait for window text.
                    Import-Module -Name $autoItModule -Force -DisableNameChecking

                    # We use loop below with 1s iteration delay (minimum for the WinWait function)
                    # to allow this task to be stoppable.
                    [int]$timeout = if ($winWait.Count -ge 3) { [int]$winWait[2] } else { [int]::MaxValue }
                    if ($timeout -le 0) {
                        $timeout = [int]::MaxValue
                    }

                    # $i - amount of seconds passed, $timeout - total amount of seconds to wait.
                    for ([int]$i = 0; $i -lt $timeout; $i++) {
                        if (Wait-AU3Win -Title "$($winWait[0])" -Text "$($winWait[1])" -Timeout 1) {
                            return
                        }
                    }

                    Write-Warning "Unable to find window in time"
                }
            }

            # Wait behavior is different if the current script is run from ISE.
            # We cannot use $Host.UI in this case, and cannot wait for a keypress, the only option available is to use
            # the 'Pause' cmdlet that doesn't accept timeouts.
            # In ISE, this script will wait for the [Enter] keypress, if the '-WinWait' parameter is not specified;
            # otherwise, it will wait for windows with text, ignoring user input (except for Ctrl-C).
            # In PowerShell, this script will wait for any keypress and for window at the same time.
            if ($ise) {
                if ($WinWait.Count -eq 0) {
                    Write-Host "`r`n **** RUN YOUR SCENARIO NOW AND PRESS [ENTER] WHEN FINISHED ****" -ForegroundColor Green
                } else {
                    Write-Host "`r`n **** RUN YOUR SCENARIO NOW, IT WILL STOP ONCE THE WINDOW IS FOUND ****" -ForegroundColor Green

                    Write-Host " **** Waiting for a window with title "      -ForegroundColor Green  -NoNewline
                    Write-Host "`"$($WinWait[0])`""                          -ForegroundColor Yellow -NoNewline
                    Write-Host " and text "                                  -ForegroundColor Green  -NoNewline
                    Write-Host "`"$($WinWait[1])`""                          -ForegroundColor Yellow -NoNewline

                    if (($WinWait.Count -gt 2) -and ($WinWait[2] -gt 0)) {
                        Write-Host " for "                                   -ForegroundColor Green  -NoNewline
                        Write-Host "$($WinWait[2]) second(s)"                -ForegroundColor Yellow -NoNewline
                    }

                    Write-Host " ****"                                       -ForegroundColor Green
                }
            } else {
                Write-Host "`r`n **** RUN YOUR SCENARIO NOW AND PRESS [ENTER] WHEN FINISHED ****" -ForegroundColor Green

                if ($WinWait.Count -gt 0) {
                    Write-Host " **** Also waiting for a window with title " -ForegroundColor Green  -NoNewline
                    Write-Host "`"$($WinWait[0])`""                          -ForegroundColor Yellow -NoNewline
                    Write-Host " and text "                                  -ForegroundColor Green  -NoNewline
                    Write-Host "`"$($WinWait[1])`""                          -ForegroundColor Yellow -NoNewline

                    if (($WinWait.Count -gt 2) -and ($WinWait[2] -gt 0)) {
                        Write-Host " for "                                   -ForegroundColor Green  -NoNewline
                        Write-Host "$($WinWait[2]) second(s)"                -ForegroundColor Yellow -NoNewline
                    }

                    Write-Host " ****"                                       -ForegroundColor Green
                }
            }

            Write-Host ""

            if (!($ise)) {
                # Need to flush before waiting for input.
                [void]$Host.UI.RawUI.Flushinputbuffer()
            }

            # Wait for user input or for UI wait job to complete.
            while ($true) {
                if ($waitJob -and $waitJob.Finished.WaitOne(0)) {
                    Write-Verbose "[Wait-ForStop] Wait job finished"
                    break
                } elseif (!($waitJob) -and $ise) {
                    Pause
                    Write-Verbose "[Wait-ForStop] [Enter] pressed"
                    break
                } elseif ($Host.UI.RawUI.KeyAvailable) {
                    $KeyPress = $host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
                    # 13 equals VK_RETURN https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx
                    if($KeyPress.VirtualKeyCode -eq 13)
                    {
                        Write-Verbose "[Wait-ForStop] Key pressed"
                        break
                    }
                    [void]$Host.UI.RawUI.Flushinputbuffer()
                }

                Start-Sleep -Milliseconds 100
            }
        } finally {
            if ($waitJob) {
                Write-Verbose "[Wait-ForStop] Stopping the wait job"

                Stop-Job    -Job $waitJob
                Receive-Job -Job $waitJob
                Remove-Job  -Job $waitJob
            }
        }
    }
}

<#
 .SYNOPSIS
 Creates required folders on the local device, if any.
#>
function Prepare-Local {
    [CmdletBinding()]
    [OutputType([void])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[Prepare-Local] Creating temporary trace folders"

        New-Item -ItemType Directory -Path $EnvironmentInfo.TracePathLocal          -Force > $null
        New-Item -ItemType Directory -Path $EnvironmentInfo.TraceScriptsPathLocal   -Force > $null

        # Copy manifests to a local directory.
        if (Test-Path "$($EnvironmentInfo.ScriptRootPath)\manifests") {
            New-Item -ItemType Directory -Path $EnvironmentInfo.TraceManifestsPathLocal -Force > $null
            Copy-Item "$($EnvironmentInfo.ScriptRootPath)\manifests\*" $EnvironmentInfo.TraceManifestsPathLocal
        }

        Write-Verbose "[Prepare-Local] Done"
    }
}

<#
 .SYNOPSIS
 Creates required folders on the target device, if any.
#>
function Prepare-Target {
    [CmdletBinding()]
    [OutputType([void])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[Prepare-Target] Creating trace folders on the target device"

        # If we are going to collect logs from the remote device, make sure it's connected.
        switch ($TargetType) {
            ([Tracing.TargetType]::TShell) {
                if (!(Test-TShellConnected)) {
                    Write-Error "[Prepare-Target] TShell connection not available"
                }

                break
            }
            ([Tracing.TargetType]::XBox) {
                if (!(Test-XBoxConnected)) {
                    Write-Error "[Prepare-Target] XBox connection not available"
                }

                break
            }
            default {}
        }

        if ($TargetType -ne [Tracing.TargetType]::Local) {
            MkDir-Target -Path $EnvironmentInfo.TracePathTarget        -TargetType ($TargetType) > $null
            MkDir-Target -Path $EnvironmentInfo.TraceScriptsPathTarget -TargetType ($TargetType) > $null
        } else {
            Write-Verbose "[Prepare-Target] Target machine is a local one, skipping"
        }

        # Copy the XPERF tool to the XBox device.
        if (($TargetType -eq [Tracing.TargetType]::XBox) -and ($ToolsetType -eq [Tracing.ToolsetType]::XPerf)) {
            # Copy either from the local or remote folder.
            if (Test-Path $EnvironmentInfo.XPerfForTarget) {
                Write-Verbose "[Prepare-Target] Copying XPerf.exe to the target device from $($EnvironmentInfo.XPerfForTarget)"
                PutFile-Target -Local $EnvironmentInfo.XPerfForTarget -Target $EnvironmentInfo.XPerfOnTarget -TargetType ($TargetType) > $null
            } elseif (Test-Path $EnvironmentInfo.XPerfRemote) {
                Write-Verbose "[Prepare-Target] Copying XPerf.exe to the target device from $($EnvironmentInfo.XPerfRemote)"
                PutFile-Target -Local $EnvironmentInfo.XPerfRemote    -Target $EnvironmentInfo.XPerfOnTarget -TargetType ($TargetType) > $null
            }
        }

        Write-Verbose "[Prepare-Target] Done"
    }
}

<#
 .SYNOPSIS
 Creates start, stop and post-processing scripts for the scenarios.
 They should be executed on the target system.
#>
function Create-Scripts {
    [CmdletBinding()]
    [OutputType([void])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState

        # Script-local lists of batch files to be executed to collect and parse the traces.
        [void]$StartScripts.Clear()
        [void]$StopScripts.Clear()
        [void]$PostProcessingScripts.Clear()
    }

    process {
        Write-Verbose "[Create-Scripts] Creating scripts for: $(($Scenario | Sort-Object -Unique) -join ',')"
        Write-Verbose "[Create-Scripts] Toolset: $ToolsetType"

        #
        # Start/stop scripts.
        #

        switch ($ToolsetType) {
            ([Tracing.ToolsetType]::XPerf) {
                Create-Scripts-Tracing-XPERF
                break
            }
            ([Tracing.ToolsetType]::Wpr) {
                Create-Scripts-Tracing-WPR
                break
            }
            default {
                Write-Error "[Create-Scripts] Unsupported toolset: $ToolsetType"
            }
        }

        #
        # Post-processing scripts.
        #

        $generatePostProcessingScripts = $true
        $currentPostProcessingType     = $PostProcessingType

        # User can globally override the post-processing steps for the scenarios chosen.
        if ($currentPostProcessingType -eq [Tracing.PostProcessingType]::NotSpecified) {
            # If post-processing steps for the scenarios are different, don't create scripts for them
            # and eventually don't do anything after the traces are collected.

            [Array]$uniquePostProcessingSteps = $Scenario | % { $ScenariosData[$_].PostProcessing } | Sort-Object -Unique

            if ($uniquePostProcessingSteps.Count -eq 1) {
                $currentPostProcessingType = Read-TracePostProcessingType $uniquePostProcessingSteps[0]

                if ($currentPostProcessingType -eq [Tracing.PostProcessingType]::NotSpecified) {
                    $generatePostProcessingScripts = $false
                    Write-Verbose "[Create-Scripts] No post-processing steps defined for the scenario(s)"
                }
            } elseif ($uniquePostProcessingSteps.Count -eq 0) {
                $generatePostProcessingScripts = $false
                Write-Verbose "[Create-Scripts] No post-processing steps defined for the scenario(s)"
            } else {
                $generatePostProcessingScripts = $false
                Write-Verbose "[Create-Scripts] Scenarios with different post-processing steps were specified, post-processing will not be run: $($uniquePostProcessingSteps -join `", `")"
            }
        } elseif ($currentPostProcessingType -eq [Tracing.PostProcessingType]::None) {
            $generatePostProcessingScripts = $false
            Write-Verbose "[Create-Scripts] User disabled the post-processing steps"
        }

        if (!($generatePostProcessingScripts)) {
            Write-Information "[Create-Scripts] No post-processing steps will be performed"
        } else {
            Write-Verbose "[Create-Scripts] Post-processing: $currentPostProcessingType"

            switch -Exact ($currentPostProcessingType) {
                { $_ -in @([Tracing.PostProcessingType]::Wpp, [Tracing.PostProcessingType]::MFTrace, [Tracing.PostProcessingType]::EPrint) } {
                    Create-Scripts-PostProcessing-Format $currentPostProcessingType
                    break
                }
                "MXA" {
                    Create-Scripts-PostProcessing-MXA $currentPostProcessingType
                    break
                }
                default {
                    Write-Error "[Create-Scripts] Unsupported post-processing type: $currentPostProcessingType"
                }
            }
        }

        Write-Verbose "[Create-Scripts] Done"
    }
}

<#
 .SYNOPSIS
 Copies the generated scripts to the target device and executes them.
#>
function Start-Tracing {
    [CmdletBinding()]
    [OutputType([void])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[Start-Tracing] Starting tracing for: $($Scenario | Sort-Object -Unique)"

        # Put scripts on device, if needed.
        if ($TargetType -ne [Tracing.TargetType]::Local) {
            PutFile-Target -Local "$($EnvironmentInfo.TraceScriptsPathLocal)\*" -Target $EnvironmentInfo.TraceScriptsPathTarget -TargetType $TargetType

            # Modify locations of the start/stop scripts, as they are on a remote device now.
            # NOTE: Don't do anything with the post-processing scripts, because they are run locally.
            $script:StartScripts = $script:StartScripts | % { "$($EnvironmentInfo.TraceScriptsPathTarget)\$((Get-Item $_).Name)" }
            $script:StopScripts  = $script:StopScripts  | % { "$($EnvironmentInfo.TraceScriptsPathTarget)\$((Get-Item $_).Name)" }
        }

        # Execute scripts.
        $StartScripts | Run-Target -TargetType $TargetType

        Write-Verbose "[Start-Tracing] Done"
    }
}

<#
 .SYNOPSIS
 Stops the logging sessions, copies result ETL files locally, and cleans up the target system.

 .PARAMETER DownloadFiles
 Whether the result files should be downloaded from the target.
#>
function Stop-Tracing {
    [CmdletBinding()]
    [OutputType([void])]
    param(
        [Parameter(Mandatory = $false, ValueFromPipeline = $true, Position = 0)]
        [Switch]
        $DownloadFiles = $true
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        try {
            $StopScripts | Run-Target -TargetType $TargetType -ErrorAction SilentlyContinue

            # If we weren't interrupted by the user, download files from the target device.
            if ($DownloadFiles) {
                # Allow some time for flush to complete and files to appear.
                [System.Threading.Thread]::Sleep(500)

                # If we are collecting local traces, there is no need to download them.
                if ($TargetType -ne [Tracing.TargetType]::Local) {
                    GetFile-Target -Target "$($EnvironmentInfo.TracePathTarget)\*" -Local $EnvironmentInfo.TracePathLocal -TargetType $TargetType

                    # Now wait a bit for the local files.
                    [System.Threading.Thread]::Sleep(500)
                }
            }
        } finally {
            # Cleanup the device.
            if ($TargetType -ne [Tracing.TargetType]::Local) {
                RmDir-Target -Path $EnvironmentInfo.TracePathTarget -TargetType $TargetType -ErrorAction Continue
            }
        }
    }
}

<#
 .SYNOPSIS
 Waits for all jobs in the 'BackgroundJobs' collection to complete.
#>
function Wait-ForBackgroundJobs {
    [CmdletBinding()]
    [OutputType([void])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[Wait-ForBackgroundJobs] Starting"

        $jobsToComplete = New-Object System.Collections.ArrayList
        [void]$jobsToComplete.AddRange($BackgroundJobs)

        while ($jobsToComplete.Count -gt 0) {
            Write-Host "`r                                        " -NoNewLine
            Write-Host "`r$($jobsToComplete.Count) job(s) left"     -NoNewLine -ForegroundColor Yellow

            $completedJob = Wait-Job -Job $jobsToComplete -Any -Timeout 240 -ErrorAction SilentlyContinue

            if ($completedJob) {
                Write-Verbose "[Wait-ForBackgroundJobs] Removing completed job from collection: $($completedJob.Name)"
                [void]$jobsToComplete.Remove($completedJob)
            } else {
                Write-Warning "Unable to wait for a job to complete"
                break
            }
        }

        Write-Host "`r                                              `r" -NoNewLine
    }

    end {
        foreach ($job in $BackgroundJobs) {
            Write-Verbose "[Wait-ForBackgroundJobs] Removing job: $($job.Name)"
            Remove-Job -Job $job -Force -ErrorAction SilentlyContinue > $null
        }

        [void]$BackgroundJobs.Clear()

        Write-Verbose "[Wait-ForBackgroundJobs] Done"
    }
}

<#
 .SYNOPSIS
 Creates the XPERF start and stop scripts for the scenario(s) chosen.
 They should be executed on the target system.
#>
function Create-Scripts-Tracing-XPERF {
    [CmdletBinding()]
    [OutputType([void])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[Create-Scripts-Tracing-XPERF] Creating tracing scripts"

        # For example: Trace_Audio_Camera_MF.
        # NOTE: The output file name should begin with the "Trace_" prefix, so the TextAnalysisTool
        #       can find and open it in the post-processing.
        $tracingSessionName    = "Trace_$(($ScenariosData.Values | % { $_.Name } | Sort-Object) -join '_')"
        $userFileName          = "$($tracingSessionName)_user.etl"
        $kernelFileName        = "$($tracingSessionName)_kernel.etl"
        $mergedFileName        = "$($tracingSessionName).etl"

        $startScriptPath       = "$($EnvironmentInfo.TraceScriptsPathLocal)\$($tracingSessionName)_start.cmd"
        $stopScriptPath        = "$($EnvironmentInfo.TraceScriptsPathLocal)\$($tracingSessionName)_stop.cmd"

        $userModeScenarios     = $ScenariosData.Values | ? { ($_.Providers) -and ($_.Providers.Count -gt 0) }
        $kernelModeScenarios   = $ScenariosData.Values | ? { $_.Kernel }
        $hasUserScenarios      = $userModeScenarios.Count   -gt 0
        $hasKernelScenarios    = $kernelModeScenarios.Count -gt 0

        # Script block to determine the location of the XPerf binary.
        $setXperfLocation      = "SET _XPERF_LOCATION_=`"$($EnvironmentInfo.XPerfOnTarget)`"`nIF NOT EXIST %_XPERF_LOCATION_% ( SET _XPERF_LOCATION_=`"$($EnvironmentInfo.XPerfRemote)`" )"

        #
        # Start script.
        #

        Write-Verbose "[Create-Scripts-Tracing-XPERF] Creating start tracing script"

        $startScript = New-Object System.Text.StringBuilder
        [void]$startScript.AppendLine("@ECHO OFF")
        [void]$startScript.AppendLine("SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION")
        [void]$startScript.AppendLine("")
        [void]$startScript.AppendLine($setXperfLocation)
        [void]$startScript.AppendLine("")

        if ($hasUserScenarios) {
            Write-Verbose "[Create-Scripts-Tracing-XPERF] Configuring user-mode tracing session"

            # Collection of user-mode providers to trace.
            $guids = New-Object System.Text.StringBuilder

            foreach ($provider in ($userModeScenarios | % { $_.Providers.Values } | Sort-Object Guid -Unique)) {
                [void]$guids.Append("$($provider.Guid):$("0x{0:X16}" -f $provider.Flags):$("0x{0:X8}" -f $provider.Level)+")
            }

            [void]$startScript.AppendLine("ECHO Starting user-mode tracing session...")
            [void]$startScript.AppendLine("%_XPERF_LOCATION_% -start `"$tracingSessionName`" -BufferSize 512 -MinBuffers 10 -MaxBuffers 64 -MaxFile 256 -FileMode Circular -FlushTimer 10 -f `"%~dp0\..\$userFileName`" -on $($guids.ToString(0, $guids.Length - 1))")
            [void]$startScript.AppendLine("")
        }

        if ($hasKernelScenarios) {
            Write-Verbose "[Create-Scripts-Tracing-XPERF] Configuring kernel-mode tracing session"

            $flags     = New-Object System.Collections.ArrayList
            $stackwalk = New-Object System.Collections.ArrayList

            foreach ($scenario in $kernelModeScenarios) {
                [void]$flags.AddRange($scenario.Kernel.Flags)
                [void]$stackwalk.AddRange($scenario.Kernel.StackWalk)
            }

            [void]$startScript.AppendLine("ECHO Starting kernel-mode tracing session...")
            [void]$startScript.AppendLine("%_XPERF_LOCATION_% -start -BufferSize 512 -MinBuffers 10 -MaxBuffers 64 -MaxFile 384 -FileMode Circular -FlushTimer 10 -f `"%~dp0\..\$kernelFileName`" -on `"$(($flags | Sort-Object -Unique) -join '+')`" -StackWalk `"$(($stackwalk | Sort-Object -Unique) -join '+')`"")
            [void]$startScript.AppendLine("")
        }

        #
        # Stop script.
        #

        Write-Verbose "[Create-Scripts-Tracing-XPERF] Creating stop tracing script"

        $stopScript = New-Object System.Text.StringBuilder
        [void]$stopScript.AppendLine("@ECHO OFF")
        [void]$stopScript.AppendLine("SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION")
        [void]$stopScript.AppendLine("")
        [void]$stopScript.AppendLine($setXperfLocation)
        [void]$stopScript.AppendLine("")

        if ($hasUserScenarios) {
            [void]$stopScript.AppendLine("ECHO Stopping user-mode tracing session...")
            [void]$stopScript.AppendLine("%_XPERF_LOCATION_% -flush `"$tracingSessionName`"")
            [void]$stopScript.AppendLine("%_XPERF_LOCATION_% -stop  `"$tracingSessionName`"")
            [void]$stopScript.AppendLine("")
        }

        if ($hasKernelScenarios) {
            [void]$stopScript.AppendLine("ECHO Stopping kernel-mode tracing session...")
            [void]$stopScript.AppendLine("%_XPERF_LOCATION_% -stop")
            [void]$stopScript.AppendLine("")
        }

        # Cook ETL files on the target.
        [void]$stopScript.AppendLine("ECHO Merging results...")
        [void]$stopScript.Append("%_XPERF_LOCATION_% -merge")

        if ($hasUserScenarios) {
            [void]$stopScript.Append(" `"%~dp0\..\$userFileName`"")
        }

        if ($hasKernelScenarios) {
            [void]$stopScript.Append(" `"%~dp0\..\$kernelFileName`"")
        }

        [void]$stopScript.AppendLine(" `"%~dp0\..\$mergedFileName`"")
        [void]$stopScript.AppendLine("")

        # Remove original ETLs to free some space.
        [void]$stopScript.AppendLine("ECHO Removing old ETL files...")

        if ($hasUserScenarios) {
            [void]$stopScript.AppendLine("DEL /Q /F `"%~dp0\..\$userFileName`"")
        }

        if ($hasKernelScenarios) {
            [void]$stopScript.AppendLine("DEL /Q /F `"%~dp0\..\$kernelFileName`"")
        }

        #
        # Save scripts to the local directory.
        #

        Write-Verbose "[Create-Scripts-Tracing-XPERF] Start script: $startScriptPath"
        Write-Verbose "[Create-Scripts-Tracing-XPERF] Stop script:  $stopScriptPath"

        $startScript.ToString() | Set-Content $startScriptPath -Encoding Ascii
        $stopScript.ToString()  | Set-Content $stopScriptPath  -Encoding Ascii

        [void]$StartScripts.Add($startScriptPath)
        [void]$StopScripts.Add($stopScriptPath)

        Write-Verbose "[Create-Scripts-Tracing-XPERF] Done"
    }
}

<#
 .SYNOPSIS
 Creates the WPR start and stop scripts for the scenarios.
 They should be executed on the target system.
#>
function Create-Scripts-Tracing-WPR {
    [CmdletBinding()]
    [OutputType([void])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[Create-Scripts-Tracing-WPR] Creating tracing scripts"

        # For example: Trace_Audio_Camera_MF.
        # NOTE: The output file name should begin with the "Trace_" prefix, so the TextAnalysisTool
        #       can find and open it in the post-processing.
        if($ScenariosData.Count -eq 0){     
            $tracingSessionName    = "Trace_$(($Scenario) -join '_')"
        }
        else{
            $tracingSessionName    = "Trace_$(($ScenariosData.Values | % { $_.Name } | Sort-Object) -join '_')"
        }

        $mergedFileName        = "$($tracingSessionName).etl"
        $wprpFileName          = "$($tracingSessionName).wprp"

        $wprpPath              = "$($EnvironmentInfo.TraceScriptsPathLocal)\$wprpFileName"
        $startScriptPath       = "$($EnvironmentInfo.TraceScriptsPathLocal)\$($tracingSessionName)_start.cmd"
        $stopScriptPath        = "$($EnvironmentInfo.TraceScriptsPathLocal)\$($tracingSessionName)_stop.cmd"

        $userModeScenarios     = $ScenariosData.Values | ? { ($_.Providers) -and ($_.Providers.Count -gt 0) }
        $kernelModeScenarios   = $ScenariosData.Values | ? { $_.Kernel }
        $hasUserScenarios      = $userModeScenarios.Count   -gt 0
        $hasKernelScenarios    = $kernelModeScenarios.Count -gt 0

        #
        # WPRP file.
        #
        if (get-command create-wprp -errorAction SilentlyContinue) {
            Write-Verbose "[Create-Scripts-Tracing-WPR] Creating WPRP session file"
            Create-WPRP -Scenario $Script:Scenario -Output $EnvironmentInfo.TraceScriptsPathLocal
        }
        else {
            Write-Verbose "[Create-Scripts-Tracing-WPR] Copy WPRP session file"
            if (Test-Path "$($EnvironmentInfo.ScriptRootPath)\wprp\$wprpFileName"){
                Copy-item "$($EnvironmentInfo.ScriptRootPath)\wprp\$wprpFileName" $EnvironmentInfo.TraceScriptsPathLocal
            }
        }

        #
        # Start sWe-Verbose "[Create-Scripts-Tracing-WPR] Creating start tracing script"

        $startScript = New-Object System.Text.StringBuilder
        [void]$startScript.AppendLine("@ECHO OFF")
        [void]$startScript.AppendLine("SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION")
        [void]$startScript.AppendLine("")

        [void]$startScript.AppendLine("ECHO Starting a new WPR session...")
        [void]$startScript.AppendLine("set WPR_LOCAL=%windir%\sysnative\wpr.exe")
        [void]$startScript.AppendLine("If Not Exist %WPR_LOCAL% ( set WPR_LOCAL=%windir%\system32\wpr.exe )")
        [void]$startScript.AppendLine("echo WPR location %WPR_LOCAL%")
        [void]$startScript.AppendLine("")

        [void]$startScript.AppendLine("%WPR_LOCAL% -flush        >nul 2>&1")
        [void]$startScript.AppendLine("%WPR_LOCAL% -resetprofint >nul 2>&1")
        [void]$startScript.AppendLine("%WPR_LOCAL% -start `"%~dp0\$wprpFileName`" -filemode")

        
        Write-Verbose "[Create-Scripts-Tracing-WPR] Start script: $startScriptPath"
        $startScript.ToString() | Set-Content $startScriptPath -Encoding Ascii
        [void]$StartScripts.Add($startScriptPath)

        #
        # Stop script.
        #

        Write-Verbose "[Create-Scripts-Tracing-WPR] Creating stop tracing script"

        $stopScript = New-Object System.Text.StringBuilder
        [void]$stopScript.AppendLine("@ECHO OFF")
        [void]$stopScript.AppendLine("SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION")
        [void]$stopScript.AppendLine("")
        [void]$stopScript.AppendLine("set WPR_LOCAL=%windir%\sysnative\wpr.exe")
        [void]$stopScript.AppendLine("If Not Exist %WPR_LOCAL% ( set WPR_LOCAL=%windir%\system32\wpr.exe )")
        [void]$stopScript.AppendLine("echo WPR location %WPR_LOCAL%")
        [void]$stopScript.AppendLine("")

        [void]$stopScript.AppendLine("ECHO Stopping the WPR session...")
        [void]$stopScript.AppendLine("%WPR_LOCAL% -flush >nul 2>&1")
        [void]$stopScript.AppendLine("%WPR_LOCAL% -stop `"%~dp0\..\$mergedFileName`"")


        Write-Verbose "[Create-Scripts-Tracing-WPR] Stop  script: $stopScriptPath"
        $stopScript.ToString()  | Set-Content $stopScriptPath  -Encoding Ascii     
        [void]$StopScripts.Add($stopScriptPath)

        Write-Verbose "[Create-Scripts-Tracing-WPR] Done"
    }
}

<#
 .SYNOPSIS
 Run background task to gather system info with DxDiag
#>
function Gather-DXDiag {
    [CmdletBinding()]
    [OutputType([void])]
    param()
    #
    # Collect information about the machine.
    #

    $dxdiagJob = Start-Job -Name "DxDiag" -ArgumentList @("$($EnvironmentInfo.TracePathLocal)\dxiag.txt") -ScriptBlock {
        param(
            [Parameter(Mandatory = $true)]
            [string]
            $OutputFile
        )

        try {
            if (Get-Command "dxdiag.exe" -ErrorAction SilentlyContinue) {
                & dxdiag /whql:off /t $OutputFile > $null
            }
        } catch {
            Write-Verbose "[Save-TargetDetails] Error while getting dxdiag logs: $_"
        }
    }

    [void]$BackgroundJobs.Add($dxdiagJob)
}

<#
 .SYNOPSIS
 Run background task to gather SetupAPIlog
#>
function Gather-SetupAPILog {

    $setupapiJob = Start-Job -Name "SetupAPI log" -ArgumentList @("$($EnvironmentInfo.TracePathLocal)") -ScriptBlock {
        param(
            [Parameter(Mandatory = $true)]
            [string]
            $OutputFile
        )

        try {
            if (Test-Path "$env:windir\inf\setupapi.dev.log" -PathType Leaf) {
                Copy-Item -Path "$env:windir\inf\setupapi.dev.log" -Destination $OutputFile
            }
        } catch {
            Write-Verbose "[Save-TargetDetails] Error while getting device installation logs: $_"
        }
    }

    [void]$BackgroundJobs.Add($setupapiJob)
}


<#
 .SYNOPSIS
 Run background task to gather system info with PnpUtil
#>
function Gather-PnpUtil {
    [CmdletBinding()]
    [OutputType([void])]
    param()
    #
    # Collect information about the machine.
    #

    $pnpUtilJob = Start-Job -Name "PnpUtil" -ArgumentList @("$($EnvironmentInfo.TracePathLocal)\pnpUtil.pnp") -ScriptBlock {
        param(
            [Parameter(Mandatory = $true)]
            [string]
            $OutputFile
        )

        try {
            if (Get-Command "pnpUtil.exe" -ErrorAction SilentlyContinue) {
                & pnpUtil /export-pnpstate $OutputFile > $null
            }
        } catch {
            Write-Verbose "[Save-TargetDetails] Error while getting pnpUtil logs: $_"
        }
    }

    [void]$BackgroundJobs.Add($pnpUtilJob)
}

