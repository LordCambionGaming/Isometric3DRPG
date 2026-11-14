# fix_glb_units.ps1 — Converte GLB da unita' Blender (cm) a glTF standard (metri)
# Moltiplica bone translations e animation translations per 0.01 (cm -> m)
# Scala le inverseBindMatrices per 0.01
#
# Uso: .\fix_glb_units.ps1 -Path "path\to\file.glb"
#      .\fix_glb_units.ps1 -Dir "path\to\dir" (processa *.glb)

param(
    [string]$Path = "",
    [string]$Dir = ""
)

function Fix-GLB {
    param([string]$FilePath)

    Write-Host "`nProcessing: $FilePath"
    $bytes = [System.IO.File]::ReadAllBytes($FilePath)

    # Leggi header GLB
    $jsonLen = [System.BitConverter]::ToUInt32($bytes, 12)
    $jsonStr = [System.Text.Encoding]::UTF8.GetString($bytes, 20, $jsonLen)
    $j = ConvertFrom-Json $jsonStr

    # Controlla se gia' fixato (Hips Z < 10 = gia' in metri)
    $skins = $j.skins
    if (-not $skins) { Write-Host "  No skins found, skipping."; return }
    $joints = $skins[0].joints
    if (-not $joints) { Write-Host "  No joints found, skipping."; return }
    $hipNode = $j.nodes[$joints[0]]
    if ($hipNode.translation) {
        $hipZ = [double]$hipNode.translation[2]
        if ([Math]::Abs($hipZ) -lt 10.0) {
            Write-Host "  Already in meters (hip.Z=$([Math]::Round($hipZ,3))), skipping."
            return
        }
        Write-Host "  Hip Z = $([Math]::Round($hipZ,3)) cm — will fix to meters"
    }

    # Determina offset del chunk binario
    $binChunkStart = 20 + $jsonLen
    $binLen = [System.BitConverter]::ToUInt32($bytes, $binChunkStart)
    $binOffset = $binChunkStart + 8  # dopo header del chunk BIN

    # Helper: leggi N float32 dall'accessor
    function Read-Floats {
        param($acc, $bufferViews, $bytes, $binOffset, $count = -1)
        $bv = $bufferViews[$acc.bufferView]
        $bvOffset = if ($bv.byteOffset) { [int]$bv.byteOffset } else { 0 }
        $accOffset = if ($acc.byteOffset) { [int]$acc.byteOffset } else { 0 }
        $byteStride = if ($bv.byteStride) { [int]$bv.byteStride } else { 0 }
        
        $typeComp = @{ 'SCALAR'=1; 'VEC2'=2; 'VEC3'=3; 'VEC4'=4; 'MAT4'=16 }
        $nComp = $typeComp[$acc.type]
        $nElem = if ($count -ge 0) { $count } else { [int]$acc.count }
        $elemSize = $nComp * 4
        $stride = if ($byteStride -gt 0) { $byteStride } else { $elemSize }
        
        $start = $binOffset + $bvOffset + $accOffset
        $result = @()
        for ($i = 0; $i -lt $nElem; $i++) {
            $off = $start + $i * $stride
            $row = @()
            for ($c = 0; $c -lt $nComp; $c++) {
                $row += [System.BitConverter]::ToSingle($bytes, $off + $c*4)
            }
            $result += ,@($row)
        }
        return $result
    }

    # Helper: scrivi N float32 nell'accessor
    function Write-Floats {
        param($acc, $bufferViews, [byte[]]$bytes, $binOffset, $values)
        $bv = $bufferViews[$acc.bufferView]
        $bvOffset = if ($bv.byteOffset) { [int]$bv.byteOffset } else { 0 }
        $accOffset = if ($acc.byteOffset) { [int]$acc.byteOffset } else { 0 }
        $byteStride = if ($bv.byteStride) { [int]$bv.byteStride } else { 0 }
        
        $typeComp = @{ 'SCALAR'=1; 'VEC2'=2; 'VEC3'=3; 'VEC4'=4; 'MAT4'=16 }
        $nComp = $typeComp[$acc.type]
        $elemSize = $nComp * 4
        $stride = if ($byteStride -gt 0) { $byteStride } else { $elemSize }
        
        $start = $binOffset + $bvOffset + $accOffset
        for ($i = 0; $i -lt $values.Count; $i++) {
            $row = $values[$i]
            $off = $start + $i * $stride
            for ($c = 0; $c -lt $nComp; $c++) {
                $fb = [System.BitConverter]::GetBytes([float]$row[$c])
                [Array]::Copy($fb, 0, $bytes, $off + $c*4, 4)
            }
        }
    }

    $bytes = [byte[]]$bytes
    $accessors = $j.accessors
    $bufferViews = $j.bufferViews
    $nodes = $j.nodes

    # Costruisci set dei joint indices
    $jointSet = @{}
    foreach ($skin in $j.skins) {
        foreach ($ji in $skin.joints) { $jointSet[[int]$ji] = $true }
    }

    # ===== 1. Modifica il JSON: scala bone node translations =====
    $fixedNodes = 0
    foreach ($ni in $jointSet.Keys) {
        $node = $nodes[$ni]
        if ($node.translation -and $node.translation.Count -ge 3) {
            $node.translation = @(
                [double]$node.translation[0] * 0.01,
                [double]$node.translation[1] * 0.01,
                [double]$node.translation[2] * 0.01
            )
            $fixedNodes++
        }
    }
    Write-Host "  Fixed $fixedNodes bone node translations in JSON"

    # ===== 2. Fix inverseBindMatrices (binario) =====
    $fixedIBM = 0
    foreach ($skin in $j.skins) {
        $ibmIdx = $skin.inverseBindMatrices
        if ($ibmIdx -eq $null) { continue }
        $acc = $accessors[[int]$ibmIdx]
        if ($acc.type -ne 'MAT4') { continue }
        $mats = Read-Floats $acc $bufferViews $bytes $binOffset
        $fixedMats = @()
        foreach ($mat in $mats) {
            $fixedMats += ,@($mat | ForEach-Object { $_ * 0.01 })
        }
        Write-Floats $acc $bufferViews $bytes $binOffset $fixedMats
        $fixedIBM += $mats.Count
    }
    Write-Host "  Fixed $fixedIBM inverseBindMatrices (x0.01)"

    # ===== 3. Fix animation translation channels (binario) =====
    $fixedChans = 0
    foreach ($anim in $j.animations) {
        $samplers = $anim.samplers
        foreach ($ch in $anim.channels) {
            if ($ch.target.path -ne 'translation') { continue }
            $nodeIdx = [int]$ch.target.node
            if (-not $jointSet.ContainsKey($nodeIdx)) { continue }
            $smp = $samplers[[int]$ch.sampler]
            $acc = $accessors[[int]$smp.output]
            if ($acc.type -ne 'VEC3') { continue }
            $vals = Read-Floats $acc $bufferViews $bytes $binOffset
            $scaled = @()
            foreach ($v in $vals) { $scaled += ,@($v[0]*0.01, $v[1]*0.01, $v[2]*0.01) }
            Write-Floats $acc $bufferViews $bytes $binOffset $scaled
            $fixedChans++
        }
    }
    Write-Host "  Fixed $fixedChans animation translation channels (x0.01)"

    # ===== Riscrive il file =====
    # Ricrea il JSON chunk con i nodes modificati
    $newJsonStr = ConvertTo-Json $j -Depth 20 -Compress
    $newJsonBytes = [System.Text.Encoding]::UTF8.GetBytes($newJsonStr)
    $pad = (4 - ($newJsonBytes.Length % 4)) % 4
    $newJsonBytes = $newJsonBytes + ([byte[]]([byte]0x20) * $pad)

    # Il chunk BIN rimane (abbiamo modificato $bytes in-place)
    $binChunkData = $bytes[$binOffset..($binOffset + $binLen - 1)]
    $binPad = (4 - ($binChunkData.Length % 4)) % 4

    $totalLen = 12 + 8 + $newJsonBytes.Length + 8 + $binChunkData.Length + $binPad
    $out = [System.Collections.Generic.List[byte]]::new()
    
    # GLB header
    $out.AddRange([System.BitConverter]::GetBytes([uint32]0x46546C67))  # magic
    $out.AddRange([System.BitConverter]::GetBytes([uint32]2))           # version
    $out.AddRange([System.BitConverter]::GetBytes([uint32]$totalLen))   # total length

    # JSON chunk
    $out.AddRange([System.BitConverter]::GetBytes([uint32]$newJsonBytes.Length))
    $out.AddRange([System.BitConverter]::GetBytes([uint32]0x4E4F534A))  # JSON
    $out.AddRange($newJsonBytes)

    # BIN chunk
    $out.AddRange([System.BitConverter]::GetBytes([uint32]($binChunkData.Length + $binPad)))
    $out.AddRange([System.BitConverter]::GetBytes([uint32]0x004E4942))  # BIN
    $out.AddRange($binChunkData)
    if ($binPad -gt 0) { $out.AddRange([byte[]]([byte]0x00) * $binPad) }

    [System.IO.File]::WriteAllBytes($FilePath, $out.ToArray())
    Write-Host "  Saved: $FilePath"
}

# Main
if ($Dir) {
    $files = Get-ChildItem -Path $Dir -Filter "*.glb" -File
    foreach ($f in $files) { Fix-GLB -FilePath $f.FullName }
} elseif ($Path) {
    Fix-GLB -FilePath $Path
} else {
    Write-Host "Uso: .\fix_glb_units.ps1 -Path file.glb"
    Write-Host "     .\fix_glb_units.ps1 -Dir directory/"
}
