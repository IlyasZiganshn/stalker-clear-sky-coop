Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$exePath = (Resolve-Path "ClearSkyCoopLauncher.exe").Path
$assembly = [System.Reflection.Assembly]::LoadFrom($exePath)
$mainFormType = $assembly.GetType("ClearSkyCoop.Launcher.MainForm")
$form = [Activator]::CreateInstance($mainFormType)

# Force handle creation and layout
$null = $form.Handle
$form.Width = 980
$form.Height = 620
$form.Show()
[System.Windows.Forms.Application]::DoEvents()

$pagesField = $mainFormType.GetField("_pages", [System.Reflection.BindingFlags]"NonPublic,Instance")
$navButtonsField = $mainFormType.GetField("_navButtons", [System.Reflection.BindingFlags]"NonPublic,Instance")
$switchPageMethod = $mainFormType.GetMethod("SwitchPage", [System.Reflection.BindingFlags]"NonPublic,Instance")
$pages = $pagesField.GetValue($form)
$navButtons = $navButtonsField.GetValue($form)

$pageNames = @("lobby", "host", "settings", "diag")

# Helper to check bounding box overlaps between visible child controls
function Check-Overlaps($container, $pageName) {
    $children = @()
    foreach ($ctrl in $container.Controls) {
        if ($ctrl.Visible -and $ctrl.Width -gt 0 -and $ctrl.Height -gt 0) {
            $children += $ctrl
            # If it's a ModernCard or panel, check its children too
            if ($ctrl.Controls.Count -gt 0) {
                Check-Overlaps $ctrl "$pageName -> $($ctrl.GetType().Name)"
            }
        }
    }
    for ($i = 0; $i -lt $children.Count; $i++) {
        for ($j = $i + 1; $j -lt $children.Count; $j++) {
            $c1 = $children[$i]
            $c2 = $children[$j]
            $r1 = [System.Drawing.Rectangle]::new($c1.Location, $c1.Size)
            $r2 = [System.Drawing.Rectangle]::new($c2.Location, $c2.Size)
            if ($r1.IntersectsWith($r2)) {
                # Ignore transparent labels or parent-child overlays if intended
                Write-Warning "OVERLAP in [$pageName]: $($c1.Text) ($($c1.GetType().Name) at $($c1.Location)) overlaps with $($c2.Text) ($($c2.GetType().Name) at $($c2.Location))"
            }
        }
    }
}

for ($i = 0; $i -lt $pages.Length; $i++) {
    # Activate page properly via SwitchPage
    $switchPageMethod.Invoke($form, @([int]$i))
    $form.PerformLayout()
    [System.Windows.Forms.Application]::DoEvents()

    Check-Overlaps $pages[$i] $pageNames[$i]

    $bmp = New-Object System.Drawing.Bitmap 980, 620
    $form.DrawToBitmap($bmp, [System.Drawing.Rectangle]::new(0, 0, 980, 620))
    $outName = "launcher_page_$($pageNames[$i]).png"
    $bmp.Save($outName, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "Rendered $outName successfully"
}

$form.Dispose()
Write-Host "All launcher pages rendered and checked for overlaps."
