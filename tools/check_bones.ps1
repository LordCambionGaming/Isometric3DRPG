$paths = @('tools/mixamo/animations/idle.fbx', 'tools/mixamo/male/base_male.fbx')
foreach ($p in $paths) {
    $bytes = [System.IO.File]::ReadAllBytes($p)
    $text = [System.Text.Encoding]::ASCII.GetString($bytes)
    $matches = [regex]::Matches($text, 'mixamorig:[a-zA-Z0-9_]+')
    $unique = @{}
    foreach ($m in $matches) {
        $unique[$m.Value] = $true
    }
    Write-Host "=== $p (mixamorig matches: $($unique.Count)) ==="
    $keys = $unique.Keys | Sort-Object
    foreach ($k in $keys) {
        Write-Host "  $k"
    }
}
