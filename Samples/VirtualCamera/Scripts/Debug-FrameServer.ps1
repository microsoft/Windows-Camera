<#
    .Synopsis
    Debug-FrameServer
    Wait for frameserver to be in running state and attach windbg to the service.

    .PARAMETER windbg
    Custom path to windbg.exe

    Example: c:\debuggers\windbg.exe
    .PARAMETER timeout
    Custom timeout wait if service doesn't start before timeout.

    Default: [int]::MaxValue
     
    .PARAMETER forcestart
    Force the service to start running 

    i.e. run net start FrameServer
        
    .PARAMETER Output
    ProcessId. The processId of frameserver, processId is invalid.

    .EXAMPLE   
    PS > Debug-FrameServer

    .EXAMPLE  
    PS > Debug-FrameServer -timeout 60 

    If frameserver doesn't start in 60s, wait will stop.

    .EXAMPLE
    PS > Debug-FrameServer -forcestart
#>
param (
        [string]$windbg,
        [int] $timeout = [int]::MaxValue,
        [switch] $forceStart
    )

# get script path 
$ScriptPath = split-path $SCRIPT:MyInvocation.MyCommand.Path -parent
. "$ScriptPath\Utils.ps1"
Debug-Service_Interal -windbg $windbg -timeout $timeout -serviceName "FrameServer" -forceStart:$forcestart