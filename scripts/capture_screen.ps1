param(
    [string]$OutputPath = "screenshot.png",
    [string]$ProcessName = ""
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = "screenshot.png"
}

$dir = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($dir) -and -not (Test-Path $dir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

$captured = $false
try {
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    if ($bounds.Width -gt 0 -and $bounds.Height -gt 0) {
        $bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
        $graphics = [System.Drawing.Graphics]::FromImage($bmp)
        $graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
        $bmp.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $graphics.Dispose()
        $bmp.Dispose()
        $captured = $true
        Write-Host "Screen screenshot saved to $OutputPath ($($bounds.Width)x$($bounds.Height))"
    }
} catch {
    Write-Warning "Direct GDI screen copy not available in current window station: $($_.Exception.Message)"
}

if (-not $captured) {
    # Fallback to latest in-game engine screenshot (.jpg) from _appdata_\screenshots
    $recentGameScreens = Get-ChildItem -Path @("local/game-host/_appdata_/screenshots", "local/game-client/_appdata_/screenshots", "D:/Games/S.T.A.L.K.E.R. Clear Sky/_appdata_/screenshots") -Filter "*.jpg" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1

    if ($recentGameScreens) {
        Copy-Item -Path $recentGameScreens.FullName -Destination $OutputPath -Force
        Write-Host "Engine in-game screenshot copied to $OutputPath ($($recentGameScreens.Name))"
        $captured = $true
    }
}
