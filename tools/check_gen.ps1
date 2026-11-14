$bytes = [System.IO.File]::ReadAllBytes('assets/models/entities/player_animated.glb')
$jsonLen = [System.BitConverter]::ToUInt32($bytes, 12)
$jsonStr = [System.Text.Encoding]::UTF8.GetString($bytes, 20, $jsonLen)
$j = ConvertFrom-Json $jsonStr
Write-Host "Generator / Extras:"
$j.asset | Out-String | Write-Host
