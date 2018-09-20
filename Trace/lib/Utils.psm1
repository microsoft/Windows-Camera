#requires -Version 4.0

<#
  .SYNOPSIS
 Contains utility functions, like starting a new process with output
 redirected, or checking whether the device is connected.
#>

#--------------------------------------------------------------------
# Variables
#--------------------------------------------------------------------

# Name of the alternate data stream name, which is used to store the
# file hash. See the 'Add-FileHash' function for details.
$StreamId = "4249DC8B-47D6-41D8-AF4E-2D95707DF50C"

# Cache for the time-consuming functions.
$IsInAdminRole            = [Nullable[bool]]$null
$IsTShellConnected        = [Nullable[bool]]$null
$IsXBoxConnected          = [Nullable[bool]]$null
$IsDomainNetworkAvailable = [Nullable[bool]]$null
$LocalSystemArchitecture  = $null

#--------------------------------------------------------------------
# Functions
#--------------------------------------------------------------------

#region Common functions

<#
 .SYNOPSIS
 Checks whether the current user is in the Administrator role.
#>
function Test-InAdminRole {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        if (!($IsInAdminRole.HasValue)) {
            $identity  = [System.Security.Principal.WindowsIdentity]::GetCurrent()
            $principal = New-Object System.Security.Principal.WindowsPrincipal($identity)

            $script:IsInAdminRole = $principal.IsInRole([System.Security.Principal.WindowsBuiltInRole]::Administrator)
        }

        return $IsInAdminRole
    }
}

<#
 .SYNOPSIS
 Checks whether the current script is running inside the PowerShell ISE.
#>
function Test-InIse {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        return $ExecutionContext.Host.name -match "ISE Host$"
    }
}

<#
 .SYNOPSIS
 Checks whether the Microsoft network resources are available.
#>
function Test-DomainNetworkAvailable {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        if (!($IsDomainNetworkAvailable.HasValue)) {
            $script:IsDomainNetworkAvailable = Test-Path "\\winbuilds\release"
        }

        return $IsDomainNetworkAvailable
    }
}

<#
 .SYNOPSIS
 Determines the current host architecture.
#>
function Get-LocalSystemArchitecture {
    [CmdletBinding()]
    [OutputType([string])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        if ($LocalSystemArchitecture -eq $null) {
            $os = Get-WmiObject Win32_OperatingSystem

            if ($os.OSArchitecture -eq "64-bit") {
                $script:LocalSystemArchitecture = "amd64"
            } else {
                $script:LocalSystemArchitecture = "x86"
            }
        }

        return $LocalSystemArchitecture
    }
}

<#
 .SYNOPSIS
 Runs the command specified, redirecting its output to the 'Write-Verbose' cmdlet.

 .PARAMETER Command
 Command to run.

 .PARAMETER ComandArgs
 Collection of arguments to pass to the $Command.
#>
function Run-Local {
    [CmdletBinding()]
    [OutputType([void])]
    param(
        [Parameter(Mandatory = $true, ValueFromPipeline = $true, Position = 0)]
        [String]
        $Command,

        [Parameter(Mandatory = $false, Position = 1)]
        [String[]]
        $CommandArgs
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[Run-Local] Executing: $Command $CommandArgs"

        $processInfo = New-Object System.Diagnostics.ProcessStartInfo
        $processInfo.FileName               = $Command
        $processInfo.Arguments              = $CommandArgs
        $processInfo.RedirectStandardError  = $true
        $processInfo.RedirectStandardOutput = $true
        $processInfo.UseShellExecute        = $false
        $processInfo.CreateNoWindow         = $true

        $process = New-Object System.Diagnostics.Process
        $process.StartInfo = $processInfo
        $process.Start()   > $null

        $outTask = $process.StandardOutput.ReadToEndAsync()
        $errTask = $process.StandardError.ReadToEndAsync()

        try {
            $process.WaitForExit() > $null

            if ($process.ExitCode -ne 0) {
                Write-Error "[Run-Local] '$Command $CommandArgs' failed with error code: $($process.ExitCode)"
            }

            [void]$outTask.AsyncWaitHandle.WaitOne(100)
            [void]$errTask.AsyncWaitHandle.WaitOne(100)

            if (($outTask.IsCompleted) -and ($outTask.Result.Length -gt 0)) {
                Write-Verbose "[Run-Local] $($outTask.Result)"
            }

            if (($errTask.IsCompleted) -and ($errTask.Result.Length -gt 0)) {
                Write-Warning "[Run-Local] $($errTask.Result)"
            }
        } catch {
            $outTask.Dispose()
            $errTask.Dispose()
            $process.Dispose()
        }

        Write-Verbose "[Run-Local] Done"
    }
}

#endregion Common functions

#region File functions

<#
 .SYNOPSIS
 Stores the $Path's hash in its alternate data stream, which makes
 it easier to find out whether the file has changed since the last
 'Add-FileHash' function call.

 .PARAMETER Path
 File to add the hash information to.
#>
function Add-FileHash {
    [CmdletBinding()]
    [OutputType([void])]
    param(
        [Parameter(Mandatory = $true, ValueFromPipeline = $true, Position = 0)]
        [String]
        $Path
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        if (!(Test-Path $Path -PathType Leaf)) {
            Write-Error "[Add-FileHash] '$Path' cannot be found"
            return
        }

        $fileHash = (Get-FileHash $Path -Algorithm SHA256).Hash

        Write-Verbose "[Add-FileHash] Storing new hash for the $($Path): $($fileHash.Hash)"

        try {
            Set-Content $Path -Stream $StreamId -Value $fileHash
        } catch {
            Write-Warning "[Add-FileHash]: $_"
        }
    }
}

<#
 .SYNOPSIS
 Checks whether the current $Path's hash matches the value stored
 by the 'Add-FileHash' function.

 .PARAMETER Path
 File to test.
#>
function Test-FileHash {
    [CmdletBinding()]
    [OutputType([bool])]
    param(
        [Parameter(Mandatory = $true, ValueFromPipeline = $true, Position = 0)]
        [String]
        $Path
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
        $result = $true
    }

    process {
        if (!(Test-Path $Path -PathType Leaf)) {
            Write-Error "[Test-FileHash] '$Path' is not a file, or it doesn't exist"
        } elseif ((Get-Item $Path).IsReadOnly) {
            Write-Verbose "[Test-FileHash] '$Path' is read-only, skipping check"
        } elseif (([System.Uri]$path).IsUnc) {
            Write-Verbose "[Test-FileHash] '$Path' is on network, skipping check"
        } else {
            $storedHash  = Get-Content $Path -Stream $StreamId -ErrorAction Ignore
            $currentHash = (Get-FileHash $Path -Algorithm SHA256).Hash

            $result = $storedHash -eq $currentHash
            Write-Verbose "[Test-FileHash] $($Path): Stored = $storedHash, Current = $currentHash, Equal = $result"
        }
    }

    end {
        return $result
    }
}

#endregion File functions

#region Validation functions

<#
 .SYNOPSIS
 Checks whether the $Data hashtable contains only the $AllowedKeys.

 .PARAMETER Data
 Hashtable to check.

 .PARAMETER AllowedKeys
 Collection of allowed keys, where the key value is whether it's mandatory:
 @{ "Data" = $true }

 .PARAMETER Name
 Optional name of the hashtable, used for logging purposes.
#>
function Validate-Hashtable {
    [CmdletBinding()]
    [OutputType([void])]
    param(
        [Parameter(Mandatory = $false, ValueFromPipeline = $true, Position = 0)]
        [AllowNull()]
        [AllowEmptyCollection()]
        [System.Collections.Hashtable]
        $Data,

        [Parameter(Mandatory = $true)]
        [System.Collections.Hashtable]
        $AllowedKeys,

        [Parameter(Mandatory = $false)]
        [String]
        $Name
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[Validate-Hashtable] Validating table with name '$Name'"

        if ($AllowedKeys.Count -eq 0) {
            Write-Verbose "[Validate-Hashtable] Allowed keys collection is empty"
        } elseif (($Data -eq $null) -or ($Data.Count -eq 0)) {
            Write-Verbose "[Validate-Hashtable] Data collection is empty"
        } else {
            # Check for unsupported keys first.
            $invalidKeys = $Data.Keys | ? { ($AllowedKeys.Keys) -notcontains $_ }
            if ($invalidKeys.Count -gt 0) {
                Write-Error "[Validate-Hashtable] Unsupported property used$(if ($Name) { ' in ' + $Name } else { '' }): $($invalidKeys -join ', ')"
            }

            # Check for mandatory keys.
            $mandatoryKeys = ($AllowedKeys.GetEnumerator() | ? { $_.Value }).Key
            $invalidKeys   = $mandatoryKeys | ? { $Data.Keys -notcontains $_ }
            if ($invalidKeys.Count -gt 0) {
                Write-Error "[Validate-Hashtable] Mandatory properties not used $(if ($Name) { ' in ' + $Name } else { '' }): $($invalidKeys -join ', ')"
            }
        }

        Write-Verbose "[Validate-Hashtable] Done"
    }
}

#endregion Validation functions

#region Remote device-aware functions

<#
 .SYNOPSIS
 Checks whether the current session window has TShell connection established.
#>
function Test-TShellConnected {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        if (!($IsTShellConnected.HasValue)) {
            $script:IsTshellConnected = ![String]::IsNullOrEmpty($DeviceAddress) -or ![String]::IsNullOrEmpty($DeviceName)
        }

        return $IsTShellConnected
    }
}

<#
 .SYNOPSIS
 Checks whether the current session window has XBox connection established.
#>
function Test-XBoxConnected {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        if (!($IsXBoxConnected.HasValue)) {
            if (Get-Command "xbrun.exe" -ErrorAction SilentlyContinue) {
                # Execute a sample XBox command.
                & xbrun /x/system /O filever /v c:\windows\system32\ntdll.dll > $null
                $script:IsXBoxConnected = $LASTEXITCODE -eq 0
            }
        }

        return $IsXBoxConnected
    }
}

<#
 .SYNOPSIS
 Checks whether the current session window has a connection with the device established.
#>
function Test-DeviceConnected {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        return ((Test-TShellConnected) -or (Test-XBoxConnected))
    }
}

<#
 .SYNOPSIS
 Runs the command with the specified args on the target device.

 .PARAMETER Command
 Command to run.

 .PARAMETER CommandArgs
 Collection of arguments to pass to the $Command.

 .PARAMETER TargetType
 Type of the target device to perform operation on.
#>
function Run-Target {
    [CmdletBinding()]
    [OutputType([void])]
    param(
        [Parameter(Mandatory = $true, ValueFromPipeline = $true, Position = 0)]
        [String]
        $Command,

        [Parameter(Mandatory = $false, Position = 1)]
        [String[]]
        $CommandArgs,

        [Parameter(Mandatory = $true)]
        [Tracing.TargetType]
        $TargetType
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[Run-Target] Executing on $($TargetType): $Command $CommandArgs"

        switch ($TargetType) {
            ([Tracing.TargetType]::TShell) {
                $commandResult = Cmd-Device $Command $CommandArgs -HideOutput

                if ($commandResult.Output) {
                    Write-Verbose "[Run-Target] $($commandResult.Output)"
                }

                if ($commandResult.ExitCode -ne 0) {
                    Write-Error "[Run-Target] '$Command $CommandArgs' failed with error code: $($commandResult.ExitCode)"
                }

                break
            }
            ([Tracing.TargetType]::XBox) {
                $output = xbrun /O "$Command $CommandArgs"

                if (!([String]::IsNullOrEmpty($output))) {
                    Write-Verbose "[Run-Target] $output"
                }

                if ($LASTEXITCODE -ne 0) {
                    Write-Error "[Run-Target] '$Command $CommandArgs' failed with error code: $LASTEXITCODE"
                }

                break
            }
            ([Tracing.TargetType]::Local) {
                Run-Local $Command $CommandArgs
            }
            default {
                Write-Error "[Run-Target] Unsupported target type: $TargetType"
            }
        }

        Write-Verbose "[Run-Target] Done"
    }
}

<#
 .SYNOPSIS
 Gets the registry value associated with the specified name on the target device.

 .PARAMETER RegKey
 Full path of the registry key.

 .PARAMETER RegName
 Name of the value to retrieve.

 .PARAMETER TargetType
 Type of the target device to perform operation on.
#>
function Get-RegistryValueTarget {
    [CmdletBinding()]
    [OutputType([String])]
    param(
        [Parameter(Mandatory = $true)]
        [String]
        $RegKey,

        [Parameter(Mandatory = $true)]
        [String]
        $RegName,

        [Parameter(Mandatory = $true)]
        [Tracing.TargetType]
        $TargetType
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
        $result = ""
    }

    process {
        Write-Verbose "[Get-RegistryValueTarget] Getting registry value for the '$RegKey'/'$RegName'"

        $LASTEXITCODE = 0

        switch ($TargetType) {
            ([Tracing.TargetType]::TShell) {
                $regQueryReturn = Reg-Device Query $RegKey /v $RegName -ErrorAction Continue
                break
            }
            ([Tracing.TargetType]::XBox) {
                $regQueryReturn = xbrun /O reg query \`"$RegKey\`" /v \`"$RegName\`"
                break
            }
            ([Tracing.TargetType]::Local) {
                $regQueryReturn = Reg Query $RegKey /v $RegName
                break
            }
            default {
                Write-Error "[Get-RegistryValueTarget] Unsupported target type: $TargetType"
            }
        }

        Write-Verbose "[Get-RegistryValueTarget] Value read: $regQueryReturn"

        if (!($Err) -and !([String]::IsNullOrEmpty($regQueryReturn)) -and ($LASTEXITCODE -eq 0)) {
            foreach ($line in $regQueryReturn) {
                if ($line -match "\sREG_\S+\s*(.*)\s*$") {
                    $result = $matches[1].ToString().Trim()
                    Write-Verbose "[Get-RegistryValueTarget] Matched: $result"
                    break
                }
            }
        }

        if ([String]::IsNullOrWhiteSpace($result)) {
            Write-Error "[Get-RegistryValueTarget] Unable to get registry value for '$RegKey'/'$RegName'"
        }

        Write-Verbose "[Get-RegistryValueTarget] Done"
    }

    end {
        return $result
    }
}

<#
 .SYNOPSIS
 Creates a folder on the target device.

 .PARAMETER Path
 Path to the directory to create.

 .PARAMETER TargetType
 Type of the target device to perform operation on.
#>
function MkDir-Target {
    [CmdletBinding()]
    [OutputType([void])]
    param(
        [Parameter(Mandatory = $true, ValueFromPipeline = $true, Position = 0)]
        [String]
        $Path,

        [Parameter(Mandatory = $true)]
        [Tracing.TargetType]
        $TargetType
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[MkDir-Target] Creating folder on the device: $Path"

        switch ($TargetType) {
            ([Tracing.TargetType]::TShell) {
                MkDir-Device $Path
                break
            }
            ([Tracing.TargetType]::XBox) {
                & xbmkdir "X$Path"
                break
            }
            ([Tracing.TargetType]::Local) {
                New-Item -ItemType Directory -Path $Path -ErrorAction Continue
                break
            }
            default {
                Write-Error "[MkDir-Target] Unsupported target type: $TargetType"
            }
        }

        Write-Verbose "[MkDir-Target] Done"
    }
}

<#
 .SYNOPSIS
 Removes the directory specified on the target device.

 .PARAMETER Path
 Path to the directory to delete.

 .PARAMETER TargetType
 Type of the target device to perform operation on.
#>
function RmDir-Target {
    [CmdletBinding()]
    [OutputType([void])]
    param(
        [Parameter(Mandatory = $true, ValueFromPipeline = $true, Position = 0)]
        [String]
        $Path,

        [Parameter(Mandatory = $true)]
        [Tracing.TargetType]
        $TargetType
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[RmDir-Target] Removing folder on the device $($TargetType): $Path"

        switch ($TargetType) {
            ([Tracing.TargetType]::TShell) {
                RmDir-Device $Path /S /Q
                break
            }
            ([Tracing.TargetType]::XBox) {
                & xbrmdir "X$Path" /F
                break
            }
            ([Tracing.TargetType]::Local) {
                Remove-Item -Path $Path -Recurse -Force -ErrorAction Continue
                break
            }
            default {
                Write-Error "[RmDir-Target] Unsupported target type: $TargetType"
            }
        }

        Write-Verbose "[RmDir-Target] Done"
    }
}

<#
 .SYNOPSIS
 Copies the file from the local to the target device.

 .PARAMETER Local
 Path to the local file to be copied.

 .PARAMETER Target
 Path to the destination file.

 .PARAMETER TargetType
 Type of the target device to perform operation on.
#>
function PutFile-Target {
    [CmdletBinding()]
    [OutputType([void])]
    param(
        [Parameter(Mandatory = $true, ValueFromPipeline = $true, Position = 0)]
        [String]
        $Local,

        [Parameter(Mandatory = $true, Position = 1)]
        [String]
        $Target,

        [Parameter(Mandatory = $true)]
        [Tracing.TargetType]
        $TargetType
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[PutFile-Target] Copying file to target device: $Local -> $Target"

        switch ($TargetType) {
            ([Tracing.TargetType]::TShell) {
                Put-Device $Local $Target | Write-Verbose -ErrorAction Continue
                break
            }
            ([Tracing.TargetType]::XBox) {
                & xbcp $Local "X$Target"
                break
            }
            ([Tracing.TargetType]::Local) {
                Copy-Item -Path $Local -Destination $Target -ErrorAction Continue
                break
            }
            default {
                Write-Error "[PutFile-Target] Unsupported target type: $TargetType"
            }
        }

        Write-Verbose "[PutFile-Target] Done"
    }
}

<#
 .SYNOPSIS
 Copies the file from the target to the local device.

 .PARAMETER Target
 Path to the file on the target to be copied.

 .PARAMETER Local
 Path to the local destination file.

 .PARAMETER TargetType
 Type of the target device to perform operation on.
#>
function GetFile-Target {
    [CmdletBinding()]
    [OutputType([void])]
    param(
        [Parameter(Mandatory = $true, ValueFromPipeline = $true, Position = 0)]
        [String]
        $Target,

        [Parameter(Mandatory = $true, Position = 1)]
        [String]
        $Local,

        [Parameter(Mandatory = $true)]
        [Tracing.TargetType]
        $TargetType
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState
    }

    process {
        Write-Verbose "[GetFile-Target] Downloading file from the target: $Target -> $Local"

        switch ($TargetType) {
            ([Tracing.TargetType]::TShell) {
                Get-Device $Target $Local -ErrorAction Continue | Write-Verbose -ErrorAction Continue
                break
            }
            ([Tracing.TargetType]::XBox) {
                & xbcp "X$Target" $Local
                break
            }
            ([Tracing.TargetType]::Local) {
                Copy-Item -Path $Target -Destination $Local -ErrorAction Continue
                break
            }
            default {
                Write-Error "[GetFile-Target] Unsupported target type: $TargetType"
            }
        }

        Write-Verbose "[GetFile-Target] Done"
    }
}

<#
 .SYNOPSIS
 Gets the target build information.
#>
function Get-BuildInfo {
    [CmdletBinding()]
    [OutputType([Tracing.BuildInfo])]
    param(
        [Parameter(Mandatory = $true, ValueFromPipeline = $true, Position = 0)]
        [Tracing.TargetType]
        $TargetType
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState

        $result = New-Object Tracing.BuildInfo
    }

    process {
        Write-Verbose "[Get-BuildInfo] Getting the target build information"

        # For example: [0] = 9600, [1] = 17630, [2] = amd64fre, [3] = winblue_r7, [4] = 150109-2022
        $buildParams = (Get-RegistryValueTarget -RegKey "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion" -RegName "BuildLabEx" -TargetType $TargetType).Split(".")

        # We use 'woa' instead of 'arm' for 32-bit.
        $buildParams[2] = $buildParams[2].Replace("armfre", "woafre")
        $buildParams[2] = $buildParams[2].Replace("armchk", "woachk")

        $result.Version = "$($buildParams[0]).$($buildParams[1]).$($buildParams[4])"  # 9600.17630.150109-2022
        $result.Flavor  = $buildParams[2]                                             # amd64fre
        $result.Branch  = $buildParams[3]                                             # winblue_r7

        Write-Verbose "[Get-BuildInfo] Version = $($result.Version), Flavor = $($result.Flavor), Branch = $($result.Branch)"
        Write-Verbose "[Get-BuildInfo] Done"
    }

    end {
        return $result
    }
}

#endregion Remote device-aware functions

#--------------------------------------------------------------------
# Script commands
#--------------------------------------------------------------------

Export-ModuleMember -Function *
