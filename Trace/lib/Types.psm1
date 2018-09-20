#requires -Version 4.0

<#
  .SYNOPSIS
 Registers new types used by the tracing script.
#>

#--------------------------------------------------------------------
# Variables
#--------------------------------------------------------------------

$typeDefs = @"
namespace Tracing
{
    public class Scenario
    {
        public string                        Name;
        public string                        Description;
        public System.Collections.Hashtable  Providers;
        public KernelSessionData             Kernel;
        public string                        PostProcessing;
    }

    public class Provider
    {
        public const ulong DefaultFlags = 0xFFFFFFFFFFFFFFFF;
        public const ulong DefaultLevel = 0xFF;

        public Provider()
        {
            this.Guid  = System.Guid.Empty;
            this.Flags = Provider.DefaultFlags;
            this.Level = Provider.DefaultLevel;
        }

        public string       Name;
        public System.Guid  Guid;
        public ulong        Flags;
        public ulong        Level;
        public string       GroupName;
        public bool         IsKernelMode;

        public Provider Clone()
        {
            return new Provider()
            {
                Name         = this.Name,
                Guid         = this.Guid,
                Flags        = this.Flags,
                Level        = this.Level,
                GroupName    = this.GroupName,
                IsKernelMode = this.IsKernelMode
            };
        }
    }

    public class KernelSessionData
    {
        public string[] Flags;
        public string[] StackWalk;
    }

    public enum OperationMode
    {
        RunEverything       = 0,
        OnlyGenerateScripts = 1,
        NoPostProcessing    = 2
    }

    public enum TargetType
    {
        Local  = 0,
        TShell = 1,
        XBox   = 2
    }

    public enum ToolsetType
    {
        XPerf = 0,
        Wpr   = 1
    }

    public enum PostProcessingType
    {
        NotSpecified = -1,
        None         = 0,
        Wpp          = 1,
        MFTrace      = 2,
        EPrint       = 3,
        Mxa          = 4
    }

    public class EnvironmentInfo
    {
        public string ScriptBinPath;
        public string TargetTemp;
        public string ScriptRootPath;

        // Extra TMF files which may be on the local machine (for privates for instance).
        public string SymLocalPath;
        public string TmfSearchPath;

        // Path to the tools used.
        public string XPerfOnTarget;
        public string XPerfForTarget;
        public string XPerfRemote;
        public string TraceFmt;
        public string TraceFmtRemote;
        public string TracePdb;
        public string TracePdbRemote;
        public string SymChk;
        public string SymChkRemote;
        public string MFTrace;
        public string MFTraceRemote;
        public string EPrint;
        public string EPrintRemote;
        public string TextAnalyzer;
        public string TextAnalyzerRemote;
        public string MXA;
        public string GetBuilds;
        public string GetBuildsRemote;

        // Output trace folders.
        public string TraceEtlBase;
        public string TracePathTarget;
        public string TracePathLocal;
        public string TraceManifestsPathLocal;
        public string TraceScriptsPathLocal;
        public string TraceScriptsPathTarget;
    }

    public class BuildInfo
    {
        public string Branch;
        public string Version;
        public string Flavor;
    }
}
"@

Add-Type -Language CSharp -ErrorAction Continue -WarningAction Continue -TypeDefinition $typeDefs

#--------------------------------------------------------------------
# Functions
#--------------------------------------------------------------------

<#
 .SYNOPSIS
 Returns the type of the operation mode user wants us to use.

 .PARAMETER Value
 String value to parse.
#>
function Read-TraceOperationMode {
    [CmdletBinding()]
    [OutputType([Tracing.OperationMode])]
    param(
        [Parameter(Mandatory = $false, ValueFromPipeline = $true, Position = 0)]
        [String]
        $Value
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState

        $result = [Tracing.OperationMode]::RunEverything
    }

    process {
        Write-Verbose "[Read-TraceOperationMode] Checking the current operation mode: $Value"

        if (!([String]::IsNullOrEmpty($Value))) {
            switch -Exact ($Value) {
                "RunEverything" {
                    $result = [Tracing.OperationMode]::RunEverything
                    break
                }
                "OnlyGenerateScripts" {
                    $result = [Tracing.OperationMode]::OnlyGenerateScripts
                    break
                }
                "NoPostProcessing" {
                    $result = [Tracing.OperationMode]::NoPostProcessing
                    break
                }
                default {
                    Write-Error "[Read-TraceOperationMode] Unknown operation mode: $Value"
                }
            }
        }

        Write-Verbose "[Read-TraceOperationMode] Operation mode type: $result"
        Write-Verbose "[Read-TraceOperationMode] Done"
    }

    end {
        return $result
    }
}

<#
 .SYNOPSIS
 Returns the type of the target system tracing should be run on.

 .PARAMETER Value
 String value to parse.
#>
function Read-TraceTargetType {
    [CmdletBinding()]
    [OutputType([Tracing.TargetType])]
    param(
        [Parameter(Mandatory = $false, ValueFromPipeline = $true, Position = 0)]
        [String]
        $Value
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState

        $result = [Tracing.TargetType]::Local
    }

    process {
        Write-Verbose "[Read-TraceTargetType] Checking the current target type: $Value"

        if (!([String]::IsNullOrEmpty($Value))) {
            switch -Exact ($Value) {
                "TShell" {
                    $result = [Tracing.TargetType]::TShell
                    break
                }
                "XBox" {
                    $result = [Tracing.TargetType]::XBox
                    break
                }
                "Local" {
                    $result = [Tracing.TargetType]::Local
                    break
                }
                default {
                    Write-Error "[Read-TraceTargetType] Unknown target type: $Value"
                }
            }
        } else {
            if (Test-TShellConnected) {
                $result = [Tracing.TargetType]::TShell
            } elseif (Test-XBoxConnected) {
                $result = [Tracing.TargetType]::XBox
            } else {
                $result = [Tracing.TargetType]::Local
            }
        }

        Write-Verbose "[Read-TraceTargetType] Target type: $result"
        Write-Verbose "[Read-TraceTargetType] Done"
    }

    end {
        return $result
    }
}

<#
 .SYNOPSIS
 Returns the type of the toolset user wants us to use.

 .PARAMETER Value
 String value to parse.
#>
function Read-TraceToolsetType {
    [CmdletBinding()]
    [OutputType([Tracing.ToolsetType])]
    param(
        [Parameter(Mandatory = $false, ValueFromPipeline = $true, Position = 0)]
        [String]
        $Value
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState

        $result = [Tracing.ToolsetType]::XPerf
    }

    process {
        Write-Verbose "[Read-TraceToolsetType] Checking the current toolset type: $Value"

        if (!([String]::IsNullOrEmpty($Value))) {
            switch -Exact ($Value) {
                "XPERF" {
                    $result = [Tracing.ToolsetType]::XPerf
                    break
                }
                "WPR" {
                    $result = [Tracing.ToolsetType]::Wpr
                    break
                }
                default {
                    Write-Error "[Read-TraceToolsetType] Unknown toolset type: $Value"
                }
            }
        }

        Write-Verbose "[Read-TraceToolsetType] Toolset type: $result"
        Write-Verbose "[Read-TraceToolsetType] Done"
    }

    end {
        return $result
    }
}

<#
 .SYNOPSIS
 Returns the type of the post-processing user wants us to use.

 .PARAMETER Value
 String value to parse.
#>
function Read-TracePostProcessingType {
    [CmdletBinding()]
    [OutputType([Tracing.PostProcessingType])]
    param(
        [Parameter(Mandatory = $false, ValueFromPipeline = $true, Position = 0)]
        [String]
        $Value
    )

    begin {
        Get-CallerPreference -Cmdlet $PSCmdlet -SessionState $ExecutionContext.SessionState

        $result = [Tracing.PostProcessingType]::NotSpecified
    }

    process {
        Write-Verbose "[Read-TracePostProcessingType] Checking the current toolset type: $Value"

        if (!([String]::IsNullOrEmpty($Value))) {
            switch -Exact ($Value) {
                "WPP" {
                    $result = [Tracing.PostProcessingType]::Wpp
                    break
                }
                "MFTrace" {
                    $result = [Tracing.PostProcessingType]::MFTrace
                    break
                }
                "EPrint" {
                    $result = [Tracing.PostProcessingType]::EPrint
                    break
                }
                "MXA" {
                    $result = [Tracing.PostProcessingType]::Mxa
                    break
                }
                "None" {
                    $result = [Tracing.PostProcessingType]::None
                    break
                }
                default {
                    Write-Error "[Read-TracePostProcessingType] Unknown post-processing type: $Value"
                }
            }
        }

        Write-Verbose "[Read-TracePostProcessingType] Post-processing type: $result"
        Write-Verbose "[Read-TracePostProcessingType] Done"
    }

    end {
        return $result
    }
}

#--------------------------------------------------------------------
# Script commands
#--------------------------------------------------------------------

Export-ModuleMember -Function *
