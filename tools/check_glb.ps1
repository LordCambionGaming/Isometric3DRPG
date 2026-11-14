$bytes = [System.IO.File]::ReadAllBytes('assets/models/characters/bases/human_male_base.glb')
$jsonLen = [System.BitConverter]::ToUInt32($bytes, 12)
$jsonStr = [System.Text.Encoding]::UTF8.GetString($bytes, 20, $jsonLen)
$j = ConvertFrom-Json $jsonStr
Write-Host "Skin joints count: $($j.skins[0].joints.Count)"
$jointNodes = $j.skins[0].joints | ForEach-Object { $j.nodes[$_].name }
Write-Host "Joint node names:"
$jointNodes | ForEach-Object { Write-Host "  $_" }

Write-Host "`nAnimations count: $($j.animations.Count)"
foreach ($a in $j.animations) {
    Write-Host "Animation: $($a.name) channels: $($a.channels.Count)"
}
