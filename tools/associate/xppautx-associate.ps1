<#
.SYNOPSIS
    Register or unregister xppautX as the handler for .ode and .odex files, for the
    current user only (HKCU\Software\Classes: no admin rights needed, and
    nothing outside this user's own registry hive is touched).

.DESCRIPTION
    Register writes a ProgID, "xppautX.Model", under HKCU\Software\Classes
    with the exe's icon and open command, and points the default value of
    .ode and .odex at it; Unregister removes them. Either way, SHChangeNotify tells Explorer
    to pick up the change without a sign-out. Run it again after moving or
    renaming xppautX.exe: the command line it wrote has the old path.

.PARAMETER Register
    Associate .ode and .odex with xppautX (the default action).

.PARAMETER Unregister
    Remove the associations (each extension only while xppautX still owns
    its default value) and the ProgID.

.PARAMETER ExePath
    Path to xppautX.exe. Defaults to xppautX.exe next to this script (the
    layout tools/associate/ sits in inside a release archive, one level
    below the top).

.PARAMETER WhatIf
    Print what would be written or removed, and do nothing.

.EXAMPLE
    powershell -File xppautx-associate.ps1 -WhatIf
    powershell -File xppautx-associate.ps1 -Register
    powershell -File xppautx-associate.ps1 -Unregister
#>
[CmdletBinding(DefaultParameterSetName = 'Register')]
param(
    [Parameter(ParameterSetName = 'Register')]
    [switch]$Register,
    [Parameter(ParameterSetName = 'Unregister')]
    [switch]$Unregister,
    [string]$ExePath,
    [switch]$WhatIf
)

$ErrorActionPreference = 'Stop'
$ProgId = 'xppautX.Model'
$Extensions = @('.ode', '.odex')
$ClassesRoot = 'HKCU:\Software\Classes'

if (-not $ExePath) {
    # tools/associate/ is one level below the release archive's top, where
    # xppautX.exe lives; a source checkout's xppautX.exe is two levels up.
    $here = Split-Path -Parent $MyInvocation.MyCommand.Path
    foreach ($candidate in @((Join-Path $here '..\xppautX.exe'), (Join-Path $here '..\..\xppautX.exe'))) {
        $resolved = $null
        try { $resolved = (Resolve-Path -LiteralPath $candidate -ErrorAction Stop).Path } catch {}
        if ($resolved) { $ExePath = $resolved; break }
    }
    if (-not $ExePath) {
        Write-Error "xppautx-associate: cannot find xppautX.exe; pass -ExePath <path to xppautX.exe>"
        exit 1
    }
} else {
    $ExePath = (Resolve-Path -LiteralPath $ExePath).Path
}

function Write-RegValue {
    param([string]$Path, [string]$Name, [string]$Value)
    if ($WhatIf) {
        $label = if ($Name) { "$Path\$Name" } else { "$Path\(Default)" }
        Write-Host "would set $label = $Value"
        return
    }
    if (-not (Test-Path -LiteralPath $Path)) { New-Item -Path $Path -Force | Out-Null }
    if ($Name) { New-ItemProperty -Path $Path -Name $Name -Value $Value -PropertyType String -Force | Out-Null }
    else { Set-Item -LiteralPath $Path -Value $Value -Force }
}

function Remove-RegKey {
    param([string]$Path)
    if ($WhatIf) {
        Write-Host "would remove $Path (if present)"
        return
    }
    if (Test-Path -LiteralPath $Path) { Remove-Item -LiteralPath $Path -Recurse -Force }
}

function Send-AssocChanged {
    if ($WhatIf) {
        Write-Host "would notify Explorer of the association change (SHChangeNotify)"
        return
    }
    $sig = @'
[DllImport("shell32.dll")]
public static extern void SHChangeNotify(int wEventId, int uFlags, IntPtr dwItem1, IntPtr dwItem2);
'@
    $type = Add-Type -MemberDefinition $sig -Name 'XppAssocNotify' -Namespace 'XppautX' -PassThru
    $SHCNE_ASSOCCHANGED = 0x08000000
    $SHCNF_IDLIST = 0x0000
    $type::SHChangeNotify($SHCNE_ASSOCCHANGED, $SHCNF_IDLIST, [IntPtr]::Zero, [IntPtr]::Zero)
}

if ($Unregister) {
    Write-Host "xppautx-associate: unregistering $($Extensions -join ' ') ($ClassesRoot)"
    foreach ($ext in $Extensions) {
        $extKey = "$ClassesRoot\$ext"
        $current = $null
        if (Test-Path -LiteralPath $extKey) {
            $item = Get-Item -LiteralPath $extKey -ErrorAction SilentlyContinue
            if ($item) { $current = $item.GetValue('') }
        }
        if ($current -eq $ProgId -or $WhatIf) {
            Remove-RegKey $extKey
        } else {
            Write-Host "$ext is not registered to $ProgId (owner: '$current'); leaving it alone"
        }
    }
    Remove-RegKey "$ClassesRoot\$ProgId"
    Send-AssocChanged
    Write-Host "done"
    exit 0
}

Write-Host "xppautx-associate: registering $($Extensions -join ' ') -> $ProgId -> `"$ExePath`" `"%1`" ($ClassesRoot)"
Write-RegValue "$ClassesRoot\$ProgId" $null 'xppautX Model'
Write-RegValue "$ClassesRoot\$ProgId\DefaultIcon" $null "`"$ExePath`",0"
Write-RegValue "$ClassesRoot\$ProgId\shell\open\command" $null "`"$ExePath`" `"%1`""
foreach ($ext in $Extensions) { Write-RegValue "$ClassesRoot\$ext" $null $ProgId }
Send-AssocChanged
Write-Host "done"
