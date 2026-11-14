$bytes = [System.IO.File]::ReadAllBytes('assets/models/characters/bases/human_male_base.glb')
$jsonLen = [System.BitConverter]::ToUInt32($bytes, 12)
$jsonStr = [System.Text.Encoding]::UTF8.GetString($bytes, 20, $jsonLen)
$j = ConvertFrom-Json $jsonStr

$rigNode = $j.nodes | Where-Object { $_.name -like '*_Rig' }
Write-Host "Rig node: $($rigNode.name)"
Write-Host "Rig scale: $($rigNode.scale -join ',')"
Write-Host "Rig rotation: $($rigNode.rotation -join ',')"

$hipNode = $j.nodes | Where-Object { $_.name -eq 'mixamorig:Hips' }
Write-Host "Hip translation: $($hipNode.translation -join ',')"
Write-Host "Hip scale: $($hipNode.scale -join ',')"

$meshAcc = $j.accessors[11]
Write-Host "Mesh POSITION acc[11] min: $($meshAcc.min -join ',') max: $($meshAcc.max -join ',')"
