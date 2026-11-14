$bytes = [System.IO.File]::ReadAllBytes('assets/models/characters/bases/human_male_base.glb')
$jsonLen = [System.BitConverter]::ToUInt32($bytes, 12)
$jsonStr = [System.Text.Encoding]::UTF8.GetString($bytes, 20, $jsonLen)
$j = ConvertFrom-Json $jsonStr

$idleAnim = $j.animations | Where-Object { $_.name -like '*Idle*' } | Select-Object -First 1
Write-Host "Idle anim: $($idleAnim.name)"
Write-Host "Channels count: $($idleAnim.channels.Count)"

$ch0 = $idleAnim.channels[0]
$node0 = $j.nodes[$ch0.target.node]
Write-Host "Ch0 target node: $($node0.name), path: $($ch0.target.path)"
$sampler0 = $idleAnim.samplers[$ch0.sampler]
$acc0 = $j.accessors[$sampler0.output]
Write-Host "Ch0 output acc: count=$($acc0.count), min=$($acc0.min -join ','), max=$($acc0.max -join ',')"

$ch1 = $idleAnim.channels[1]
$node1 = $j.nodes[$ch1.target.node]
Write-Host "Ch1 target node: $($node1.name), path: $($ch1.target.path)"
$sampler1 = $idleAnim.samplers[$ch1.sampler]
$acc1 = $j.accessors[$sampler1.output]
Write-Host "Ch1 output acc: count=$($acc1.count), min=$($acc1.min -join ','), max=$($acc1.max -join ',')"
