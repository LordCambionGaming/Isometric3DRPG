$bytes = [System.IO.File]::ReadAllBytes('assets/models/characters/bases/human_male_base.glb')
$jsonLen = [System.BitConverter]::ToUInt32($bytes, 12)
$jsonStr = [System.Text.Encoding]::UTF8.GetString($bytes, 20, $jsonLen)
$j = ConvertFrom-Json $jsonStr

Write-Host "Total nodes: $($j.nodes.Count)"
for ($i = 0; $i -lt $j.nodes.Count; $i++) {
    $n = $j.nodes[$i]
    if ($n.name -like '*Rig*' -or $n.name -like '*Hips*' -or $n.name -like '*basemesh*') {
        Write-Host "Node[$i] '$($n.name)':"
        Write-Host "  children: $($n.children -join ',')"
        Write-Host "  mesh: $($n.mesh)"
        Write-Host "  scale: $($n.scale -join ',')"
        Write-Host "  rotation: $($n.rotation -join ',')"
        Write-Host "  translation: $($n.translation -join ',')"
    }
}
Write-Host "Scene nodes: $($j.scenes[0].nodes -join ',')"
