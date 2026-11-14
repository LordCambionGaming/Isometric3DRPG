$bytes = [System.IO.File]::ReadAllBytes('assets/models/entities/player_animated.glb')
$jsonLen = [System.BitConverter]::ToUInt32($bytes, 12)
$jsonStr = [System.Text.Encoding]::UTF8.GetString($bytes, 20, $jsonLen)
$j = ConvertFrom-Json $jsonStr
$j.materials | Select-Object name | Format-Table -AutoSize
