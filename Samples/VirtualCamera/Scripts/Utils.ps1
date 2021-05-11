function Write-Log (
    [String] $message, 
    [string] $logStyle, 
    [int]    $stackIdx=1
    )
{
    <#
    .DESCRIPTION 

    Write log lines into text file: $CameraUtils_LogPath\VirtualAssist_Camera.log
    Each log line is appended with function info based on the stack index.

    .INPUTS
    String. $message - text log

    String. $logStyle - not used (use by overload function)

    Int.    $stackIdx - Index in stack which function information will be log.
    #>
    
    try
    {
        $timestamp = [System.DateTime]::Now.ToString("MMddyyyy-HH:mm:ss") 

        # format: [script.ps1(lineNumber):FunctionName]
        $stack = Get-PSCallStack
        $funInfo = ""
        if($stackIdx -lt $stack.count)
        {
            $funInfo = "[$($stack[$stackIdx].ScriptName)($($stack[$stackIdx].ScriptLineNumber)):$($stack[$stackIdx].FunctionName)]"
        }

        # write log to console
        $textStyle = ""
        switch($logStyle)
        {
            "ERROR"  { $textStyle = "-ForegroundColor ([System.ConsoleColor]::Red)";    break }
            "WARN"   { $textStyle = "-ForegroundColor ([System.ConsoleColor]::yellow)"; break }
            "INFO"   { $textStyle = "-ForegroundColor ([System.ConsoleColor]::Green)";  break }
            default  { $textStyle = $logStyle;  break }
        }

        $_message = $message.replace('"', '""')
        $cmd = "write-host ""$_message"" $textStyle"
        Invoke-Expression $cmd
    }
    catch
    {
        Write-Error "Caught Exception! $message"
        Write-Error "Error: $($_.Exception.Message)" 
        Write-Error "Stack: `n$($_.ScriptStackTrace)"
    }
}

function Log-Exception([String] $message)
{
    Write-Log "$message"      -logStyle Error -stackIdx 2
    Write-Log "Error: $($_.Exception.Message)"  -logStyle Error -stackIdx 2
    Write-Log "Stack: `n$($_.ScriptStackTrace)" -logStyle Error -stackIdx 2
}

function Debug-Service_Interal
{
    param (

        [string]$windbg,
        [int]   $timeout      = [int]::MaxValue,
        [string]$serviceName,
        [switch]$forceStart
    )

    #########################################################
    ## MAIN
    #########################################################
    Add-Type -assemblyname System.ServiceProcess
    $windbgPath = ""
    if([String]::IsNullOrEmpty($windbg) -eq $true)
    {
        $defaultWindbg = get-command "windbg.exe" 2>$null
        if($defaultWindbg -eq $null)
        {
            Write-Log "Cannot find windbg on the system.  Please provide the installed path -windbg [installpath]\windbg.exe" -logStyle Error
            Write-Log "or you can download and install from https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/debugger-download-tools" -logStyle Error
            return
        }
        $windbgPath = $defaultWindbg.source
    }
    else
    {
        if((test-path $windbg) -eq $false)
        {
            Write-Log "Cannot find windbg: $windbg" -logStyle Error
            return
        }
        $windbgPath = $windbg
    }

    $debugFSJob_Name = "debugFS_Job"

    try
    {  
        $initScriptBlock = [scriptblock]::Create(" `
            function Write-Log {$((Get-Item ""function:Write-Log"").ScriptBlock)} ` 
            function Log-Exception {$((Get-Item ""function:Log-Exception"").ScriptBlock)} `
            ")
        
        $debugFSJob = Start-Job -Name $debugFSJob_Name -ArgumentList @($windbgPath, $timeout, $serviceName) -InitializationScript $initScriptBlock  -ScriptBlock {
            param(
                [string] $windbgPath,
                $timeout_s,
                $serviceName
                )
        
            $processId = 0
            try 
            {
                Add-Type -assemblyname System.ServiceProcess
                [ServiceProcess.ServiceControllerStatus] $status = [ServiceProcess.ServiceControllerStatus]::Running
                $timeout_slice = 10;
                $loopCount = [int]($timeout_s / $timeout_slice);

                [ServiceProcess.ServiceController]$sc = New-Object "ServiceProcess.ServiceController" $serviceName

                for($it = 0; $it -lt $loopCount; $it++) 
                {
                    [TimeSpan]$timeOut = New-Object TimeSpan(0,0,0,$timeout_slice,0)
                    
                    # WaitFor-ServiceStatus with timeout
                    $sc.WaitForStatus($status, $timeOut)
                    if($sc.Status -eq $status)
                    {
                        $processId = Get-WmiObject -Class Win32_Service | where {$_.Name -EQ "$serviceName"} | Select-Object -ExpandProperty ProcessId
                        $processId >> out.log
                        Write-Log "$windbgPath -p $processId"
                        Invoke-Expression "$windbgPath -p $processId" #attached debugger to service process
                        return $processId
                    }
                }
            } 
            catch 
            {
                Log-Exception "[debugFS] Error trying to debug service: $serviceName"
                return $processId
            }
            return $processId
        }

        $firstLoop = $true
        $continue = $true
        $KeyOption = 's','S'
        $processId = 0
     
        while ($continue -eq $true)
        {
            $host.UI.RawUI.FlushInputBuffer()

            $completedJob = Wait-Job -Job $debugFSJob -Any -Timeout 1 -ErrorAction SilentlyContinue
 
            if ($completedJob) 
            {
                $processId = $completedJob | Receive-Job
                if($processId -eq 0) 
                {
                    Write-Log "Unable to get FrameServer processId"
                }

                $continue = $false
            } 
            elseif ($firstLoop)
            {
                Write-Log "`r`n **** Press S to stop ****" -logStyle Info

                if($forceStart)
                {
                    net start $serviceName
                }
                $firstLoop = $false
            } 
            elseif ($host.UI.RawUI.KeyAvailable -eq $true)
            {
                $KeyPress = $host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
                if($KeyOption -Contains $KeyPress.Character)
                {
                    Write-Log "exit"
                    $continue = $false
                }
            }
        }
        
        Write-Log "FrameServer ProcessId = $processId, Exit"
   
        Get-job $debugFSJob_Name | Foreach $_ {remove-job $_ -force}
    }
    catch
    {
        Log-Exception
        Get-job $debugFSJob_Name | Foreach $_ {remove-job $_ -force}
    }
}
