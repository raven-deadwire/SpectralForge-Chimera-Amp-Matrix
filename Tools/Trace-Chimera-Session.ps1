#requires -Version 5.1
# Optional local diagnostics. Run in a normal 64-bit Windows PowerShell window.
[CmdletBinding()]
param(
    [string]$HostExecutable,
    [string]$OutputDirectory = (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Chimera Diagnostics'),
    [switch]$CaptureDump
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or ![Environment]::Is64BitProcess) {
    throw 'Use a normal 64-bit PowerShell window on Windows. Administrator privileges are not required.'
}

if ([string]::IsNullOrWhiteSpace($HostExecutable)) {
    Add-Type -AssemblyName System.Windows.Forms
    $picker = New-Object System.Windows.Forms.OpenFileDialog
    try {
        $picker.Title = 'Select the installed Studio One executable'
        $picker.Filter = 'Studio One executable (*.exe)|*.exe'
        $picker.CheckFileExists = $true
        if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { return }
        $HostExecutable = $picker.FileName
    } finally { $picker.Dispose() }
}
$hostPath = (Resolve-Path -LiteralPath $HostExecutable).ProviderPath
if (!(Test-Path -LiteralPath $hostPath -PathType Leaf) -or [IO.Path]::GetExtension($hostPath) -ne '.exe') {
    throw 'Select the installed Studio One .exe file.'
}
$hostName = [IO.Path]::GetFileNameWithoutExtension($hostPath)
$existing = @(Get-Process | Where-Object { $_.ProcessName -eq $hostName -or $_.ProcessName -like 'Studio One*' })
if ($existing.Count -gt 0) {
    throw 'Close every Studio One process before running this helper. The helper does not close or terminate any process.'
}

$sessionName = 'Chimera-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 6)
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
$sessionPath = Join-Path $outputRoot $sessionName
New-Item -ItemType Directory -Path $sessionPath -Force | Out-Null
$tracePath = Join-Path $sessionPath 'Chimera-lifecycle.log'
[IO.File]::WriteAllText($tracePath, '')

# Only this newly launched process and its children inherit the trace setting.
# Do not modify the caller's environment, registry, host settings or project.
$start = New-Object Diagnostics.ProcessStartInfo
$start.FileName = $hostPath
$start.WorkingDirectory = [IO.Path]::GetDirectoryName($hostPath)
$start.UseShellExecute = $false
$start.EnvironmentVariables['CHIMERA_LIFECYCLE_TRACE'] = $tracePath
$hostProcess = [Diagnostics.Process]::Start($start)
if ($null -eq $hostProcess) { throw 'Studio One did not start.' }
$session = [ordered]@{
    started_utc = [DateTime]::UtcNow.ToString('o')
    host_executable = [IO.Path]::GetFileName($hostPath)
    host_version = [Diagnostics.FileVersionInfo]::GetVersionInfo($hostPath).FileVersion
    host_pid = $hostProcess.Id
    dump_requested = [bool]$CaptureDump
    dump_written = $false
    dump_error = $null
    trace_records = 0
    collected_utc = $null
}
Write-Host "Local diagnostic folder: $sessionPath"
Write-Host 'Open a COPY of the affected song/project, then reproduce the project-close problem.'
Write-Host 'Keep this PowerShell window open. The helper will never terminate Studio One.'

try {
    if ($CaptureDump) {
        Write-Host 'The optional small dump contains thread stacks and can include in-memory strings or paths.'
        $answer = Read-Host 'After the hang occurs, press Enter to capture the dump; type SKIP for logs only'
        if ($answer.Length -eq 0) {
            $hostProcess.Refresh()
            if ($hostProcess.HasExited) {
                $session.dump_error = 'The launched host has already exited; no other process was attached.'
                Write-Warning $session.dump_error
            } else {
                $dumpPath = Join-Path $sessionPath 'Studio-One-threads.dmp'
                try {
                    if ($null -eq ('ChimeraDiagnostics.SmallDump' -as [type])) {
                        Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
namespace ChimeraDiagnostics {
    public static class SmallDump {
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr OpenProcess(uint access, bool inherit, uint processId);
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CloseHandle(IntPtr handle);
        [DllImport("dbghelp.dll", SetLastError = true)]
        [DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool MiniDumpWriteDump(IntPtr process, uint processId,
            IntPtr file, uint type, IntPtr exception, IntPtr userStreams, IntPtr callback);
        public static void Write(int processId, string path) {
            // PROCESS_QUERY_INFORMATION | PROCESS_VM_READ; no debug privilege.
            IntPtr process = OpenProcess(0x0410, false, (uint)processId);
            if (process == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
            try {
                using (var file = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None)) {
                    // MiniDumpNormal | MiniDumpWithUnloadedModules | MiniDumpWithThreadInfo.
                    // Deliberately omit MiniDumpWithFullMemory and memory data segments.
                    if (!MiniDumpWriteDump(process, (uint)processId, file.SafeFileHandle.DangerousGetHandle(),
                        0x1020, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero))
                        throw new Win32Exception(Marshal.GetLastWin32Error());
                }
            } finally { CloseHandle(process); }
        }
    }
}
'@
                    }
                    [ChimeraDiagnostics.SmallDump]::Write($hostProcess.Id, $dumpPath)
                    $session.dump_written = $true
                    Write-Host 'Small thread dump saved locally.'
                } catch {
                    $session.dump_error = $_.Exception.Message
                    Write-Warning ("Dump capture failed; the lifecycle log will still be saved. " + $session.dump_error)
                    if ($dumpPath -and (Test-Path -LiteralPath $dumpPath)) {
                        Remove-Item -LiteralPath $dumpPath
                    }
                }
            }
        }
    } else {
        Read-Host 'After testing project close (or observing the hang), press Enter to collect the log' | Out-Null
    }

    $session.trace_records = @(Get-Content -LiteralPath $tracePath).Count
    $session.collected_utc = [DateTime]::UtcNow.ToString('o')
    $session | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $sessionPath 'session.json') -Encoding UTF8
    if ($session.trace_records -eq 0) {
        Write-Warning 'No Chimera lifecycle records were received. Check that this diagnostic build of the VST3 was loaded.'
    }
    $bundlePath = Join-Path $outputRoot ($sessionName + '.zip')
    Compress-Archive -LiteralPath $sessionPath -DestinationPath $bundlePath -CompressionLevel Optimal
    Write-Host "Saved local diagnostic bundle: $bundlePath"
    Write-Host 'Nothing was uploaded. The ZIP is a snapshot; later events remain in the original log folder.'
} finally {
    # Dispose releases this script's process handle; it does not stop the DAW.
    $hostProcess.Dispose()
}
