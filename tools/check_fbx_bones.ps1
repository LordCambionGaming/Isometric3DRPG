$bytes = [System.IO.File]::ReadAllBytes('tools/mixamo/male/base_male.fbx')
$text = [System.Text.Encoding]::ASCII.GetString($bytes)
$matches = [regex]::Matches($text, 'mixamorig:[a-zA-Z0-9_]+')
Write-Host "Total mixamorig matches: $($matches.Count)"
$unique = $matches | ForEach-Object { $_.Value } | Sort-Object -Unique
foreach ($u in $unique) {
    Write-Host "  $u"
}
