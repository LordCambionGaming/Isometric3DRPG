$bytes = [System.IO.File]::ReadAllBytes('assets/models/characters/bases/human_male_base.glb')
$jsonLen = [System.BitConverter]::ToUInt32($bytes, 12)
$jsonStr = [System.Text.Encoding]::UTF8.GetString($bytes, 20, $jsonLen)
$j = ConvertFrom-Json $jsonStr

$idleAnim = $j.animations | Where-Object { $_.name -like '*Idle*' } | Select-Object -First 1

for ($i = 0; $i -lt 10; $i++) {
    $ch = $idleAnim.channels[$i]
    $node = $j.nodes[$ch.target.node]
    Write-Host "Ch[$i] target: $($node.name), path: $($ch.target.path)"
}
