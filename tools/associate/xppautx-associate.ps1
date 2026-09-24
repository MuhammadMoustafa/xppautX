<#
.SYNOPSIS
    Register or unregister xppautX as the handler for .ode files, for the
    current user only (HKCU\Software\Classes: no admin rights needed, and
    nothing outside this user's own registry hive is touched).

.DESCRIPTION
    Register writes a ProgID, "xppautX.Model", under HKCU\Software\Classes
    with the exe's icon and open command, and points .ode's default value at
    it; Unregister removes both. Either way, SHChangeNotify tells Explorer
    to pick up the change without a sign-out. Run it again after moving or
    renaming xppautX.exe: the command line it wrote has the old path.

.PARAMETER Register
    Associate .ode with xppautX (the default action).

.PARAMETER Unregister
    Remove the association (and the ProgID, if xppautX still owns .ode's
    default value).

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
    Write-Host "xppautx-associate: unregistering .ode ($ClassesRoot)"
    $odeKey = "$ClassesRoot\.ode"
    $current = $null
    if (Test-Path -LiteralPath $odeKey) {
        $item = Get-Item -LiteralPath $odeKey -ErrorAction SilentlyContinue
        if ($item) { $current = $item.GetValue('') }
    }
    if ($current -eq $ProgId -or $WhatIf) {
        Remove-RegKey "$ClassesRoot\.ode"
    } else {
        Write-Host ".ode is not registered to $ProgId (owner: '$current'); leaving it alone"
    }
    Remove-RegKey "$ClassesRoot\$ProgId"
    Send-AssocChanged
    Write-Host "done"
    exit 0
}

Write-Host "xppautx-associate: registering .ode -> $ProgId -> `"$ExePath`" `"%1`" ($ClassesRoot)"
Write-RegValue "$ClassesRoot\$ProgId" $null 'xppautX Model'
Write-RegValue "$ClassesRoot\$ProgId\DefaultIcon" $null "`"$ExePath`",0"
Write-RegValue "$ClassesRoot\$ProgId\shell\open\command" $null "`"$ExePath`" `"%1`""
Write-RegValue "$ClassesRoot\.ode" $null $ProgId
Send-AssocChanged
Write-Host "done"
