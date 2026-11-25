param(
    [Parameter(Mandatory)][string]$GameDirectory,
    [Parameter(Mandatory)][string]$PhobosPackageDirectory,
    [Parameter(Mandatory)][string]$StandaloneSpawner,
    [ValidateRange(2,3)][int]$PlayerCount=2,
    [ValidateRange(1024,65527)][int]$Port=31001,
    [int]$Seed=1804805662,
    [int]$TimeoutSeconds=180,
    [ValidateSet('Direct','Impaired','ImpairedStartup')][string]$NetworkProfile='Direct',
    [switch]$MountsOnly,
    [switch]$ActSp,
    [switch]$InjectDivergence,
    [switch]$SwapPlayers,
    [switch]$KeepGameAssets,
    [switch]$SkipBuild,
    [switch]$Rebuild,
    [switch]$CopyGameAssets,
    [string]$ReportPath
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'lib/Common.ps1')
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$package=(Resolve-Path -LiteralPath $PhobosPackageDirectory).Path
& (Join-Path $PSScriptRoot 'build.ps1') -Case multiplayer -Rebuild:$Rebuild -CheckOnly:$SkipBuild
if($ActSp) { & (Join-Path $PSScriptRoot 'build.ps1') -Case actsp -CheckOnly:$SkipBuild }
$run=Join-Path $PSScriptRoot ('local/runs/multiplayer-'+[guid]::NewGuid().ToString('N'))
$null=New-Item -ItemType Directory $run
Write-Output "Multiplayer directory: $run"
$processes=@()
$dirs=@()
$relay=$null
$compared=0
function Compare-Samples {
    $files=@($dirs | ForEach-Object {Join-Path $_ 'snapshots.csv'})
    if($files.Count -ne $PlayerCount -or !(Test-Path -LiteralPath $files[0]) -or !(Test-Path -LiteralPath $files[1])) {return}
    $left=@(Get-Content -LiteralPath $files[0] | Where-Object {$_ -match '^\d+,[0-9a-f]+,[0-9a-f]+$'})
    $limit=$left.Count
    foreach($file in $files[1..($PlayerCount-1)]) {
        if(!(Test-Path -LiteralPath $file)) {return}
        $right=@(Get-Content -LiteralPath $file | Where-Object {$_ -match '^\d+,[0-9a-f]+,[0-9a-f]+$'})
        $limit=[Math]::Min($limit,$right.Count)
        for($sample=$script:compared;$sample -lt $limit;$sample++) {
            if($left[$sample] -ne $right[$sample]) {throw "Peers differ at $file : $($left[$sample]) versus $($right[$sample]); $run"}
        }
    }
    $script:compared=$limit
}
try {
    foreach ($peer in 0..($PlayerCount-1)) {
        $dir=Join-Path $run ('peer'+$peer)
        $null=New-Item -ItemType Directory $dir
        $null=New-Item -ItemType File (Join-Path $dir 'Starfall.test')
        if($MountsOnly) {$null=New-Item -ItemType File (Join-Path $dir 'mount-network.flag')}
        if($NetworkProfile -ne 'Direct') {$null=New-Item -ItemType File (Join-Path $dir 'impaired.flag')}
        $dirs+=$dir
        Copy-TestGameFiles $game $dir -CopyArchives:$CopyGameAssets
        foreach ($name in 'Phobos.dll','Syringe.exe','gamemd.edb') { Copy-Item -LiteralPath (Join-Path $package $name) -Destination $dir -Force }
        Copy-Item -LiteralPath (Join-Path $root 'build/tests/multiplayer/Starfall.dll') -Destination $dir
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'game/multiplayer/Starfall.ini') -Destination $dir
        $scripts=Get-Item -LiteralPath $dir
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'game/multiplayer/Network.lua') -Destination $scripts.FullName
        $map=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'maps/Starfall_TestRange.map'))
        # Retain terrain and spawn positions; remove the single-player target range
        # combatants and triggers so they cannot destroy network fixture objects.
        $map=[regex]::Replace($map,'(?ms)^\[(?:Units|Infantry|Structures|Triggers|Tags|Events|Actions|TeamTypes|TaskForces|ScriptTypes)\]\r?\n.*?(?=^\[|\z)','')
        if($PlayerCount -eq 3) {$map=$map.Replace('MaxPlayer=2','MaxPlayer=3').Replace('1=48075', "1=48075`r`n2=80076")}
        if($ActSp) {
            Copy-Item -LiteralPath (Join-Path $root 'build/ActSp.dll') -Destination $dir
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'game/actsp/rules_include.ini') -Destination $dir
            Copy-Item -LiteralPath (Join-Path $root 'examples/GrizzlyGroundOutpost/GrizzlyGround.rules.ini') -Destination $dir
            $map+=[Environment]::NewLine+[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'game/actsp/Network.ini'))
        }
        [IO.File]::WriteAllText((Join-Path $dir 'spawnmap.ini'),$map)
        if($InjectDivergence -and $peer -eq 1) {$null=New-Item -ItemType File (Join-Path $dir 'diverge.flag')}
        $spawn=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'game/spawn.ini'))
        $color=if($SwapPlayers) {6-$PlayerCount+$peer} else {5-$peer}
        $spawn=$spawn.Replace('PlayerCount=1',('PlayerCount='+$PlayerCount)).Replace('AIPlayers=1','AIPlayers=0').Replace('UnitCount=10','UnitCount=0').Replace('Color=5',('Color='+$color)).Replace('Seed=1804805662',('Seed='+$Seed))
        $spawn=[regex]::Replace($spawn,'(?s)\[HouseHandicaps\].*?(?=\[SpawnLocations\])','')
        $spawn=$spawn.Replace('Name=5b4e657720506c61',('Name=StarfallPeer'+$peer))
        $spawn=$spawn.Replace('[Settings]',("[Settings]`r`nPort="+($Port+$peer)+"`r`nFrameSendRate=3`r`nMaxAhead=12`r`nProtocol=0`r`n"))
        if($PlayerCount -eq 3) {$spawn=$spawn.Replace('Multi2=1',"Multi2=1`r`nMulti3=2")}
        $slot=0
        foreach($other in 0..($PlayerCount-1)) {
            if($other -eq $peer) {continue}
            $otherPort=if($NetworkProfile -ne 'Direct') {$Port+$PlayerCount+$peer*($PlayerCount-1)+$slot} else {$Port+$other}
            $otherColor=if($SwapPlayers) {6-$PlayerCount+$other} else {5-$other}
            $slot++
            $spawn+="`r`n[Other$slot]`r`nName=StarfallPeer$other`r`nSide=0`r`nColor=$otherColor`r`nIp=127.0.0.1`r`nPort=$otherPort`r`nIsSpectator=False`r`n"
        }
        [IO.File]::WriteAllText((Join-Path $dir 'spawn.ini'),$spawn)
        [IO.File]::AppendAllText((Join-Path $dir 'RA2MD.ini'),"`r`n[Debug]`r`nSkipCreateAppMutex=yes`r`nMPDebug=yes`r`nMPDebug.Show=no`r`n")
        Copy-Item -LiteralPath $StandaloneSpawner -Destination (Join-Path $dir 'CnCNet-Spawner.dll')
        $modules=@('Starfall.dll','Ares.dll','CnCNet-Spawner.dll','Phobos.dll')
        if($ActSp) {
            & (Join-Path $PSScriptRoot 'check-hook-ranges.ps1') -Primary (Join-Path $dir 'ActSp.dll') -Extensions @($modules | ForEach-Object {Join-Path $dir $_})
            $modules=@('Starfall.dll','ActSp.dll')+$modules[1..3]
        }
        & (Join-Path $PSScriptRoot 'check-hook-ranges.ps1') -Primary (Join-Path $dir 'Starfall.dll') -Extensions @($modules | Select-Object -Skip 1 | ForEach-Object {Join-Path $dir $_})
    }
    $identities=@('Starfall.dll','Ares.dll','Phobos.dll','gamemd.exe','Syringe.exe','Starfall.ini','spawnmap.ini','Network.lua')
    if($ActSp) {$identities+=@('ActSp.dll','rules_include.ini','GrizzlyGround.rules.ini')}
    $identities+='CnCNet-Spawner.dll'
    $fingerprints=@(foreach($name in $identities) {
        $left=(Get-FileHash -LiteralPath (Join-Path $dirs[0] $name)).Hash
        foreach($dir in $dirs[1..($PlayerCount-1)]) {
            if($left -ne (Get-FileHash -LiteralPath (Join-Path $dir $name)).Hash) {throw "Peer input differs: $dir/$name"}
        }
        [pscustomobject]@{File=$name;SHA256=$left}
    })
    [pscustomobject]@{Seed=$Seed;SwappedPlayers=[bool]$SwapPlayers;NetworkProfile=$NetworkProfile;InjectedDivergence=[bool]$InjectDivergence;IdenticalInputs=$fingerprints} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $run 'inputs.json')
    if($NetworkProfile -ne 'Direct') {
        $relayScript=Join-Path $PSScriptRoot 'network/run-relay.ps1'
        $relay=Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList @('-NoProfile','-File',('"'+$relayScript+'"'),'-Port',$Port,'-Profile',$NetworkProfile,'-PlayerCount',$PlayerCount,'-Directory',('"'+$run+'"')) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'relay-stdout.log') -RedirectStandardError (Join-Path $run 'relay-stderr.log')
        $readyDeadline=(Get-Date).AddSeconds(15)
        while(!(Test-Path (Join-Path $run 'relay.ready')) -and !$relay.HasExited -and (Get-Date) -lt $readyDeadline) {Start-Sleep -Milliseconds 100}
        if(!(Test-Path (Join-Path $run 'relay.ready'))) {throw "Loopback relay failed: $run"}
    }
    foreach($dir in $dirs) {
        $imports=($modules | ForEach-Object {'-i='+$_}) -join ' '
        $arguments='gamemd.exe '+$imports+' --args="-SPAWN -LOG -CD" --nodetach'
        $processes+=Start-Process -FilePath (Join-Path $dir 'Syringe.exe') -ArgumentList $arguments -WorkingDirectory $dir -WindowStyle Hidden -PassThru
        Start-Sleep -Seconds 2
    }
    $watch=[Diagnostics.Stopwatch]::StartNew()
    do {
        Start-Sleep -Milliseconds 250
        $active=@($processes | Where-Object {!$_.HasExited})
        if($relay -and $relay.HasExited) {throw "Loopback relay exited unexpectedly: $run"}
        Compare-Samples
        foreach($dir in $dirs) {
            $log=Join-Path $dir 'multiplayer.log'
            if((Test-Path -LiteralPath $log) -and (Get-Content -LiteralPath $log -Raw) -match '(?m)^FAIL[^\r\n]*') {throw "$dir`: $($Matches[0])"}
            $loaderLog=Join-Path $dir 'syringe.log'
            if((Test-Path -LiteralPath $loaderLog) -and (Get-Content -LiteralPath $loaderLog -Raw) -match 'Done with exit code (?!0 \(0\))\w+ \(') {throw "Game exited abnormally: $dir"}
            if(@(Get-ChildItem -LiteralPath (Join-Path $dir 'debug') -Filter 'except.txt' -Recurse -ErrorAction SilentlyContinue).Count) {throw "Game generated a crash report: $dir"}
            if(@(Get-ChildItem -LiteralPath $dir -Filter 'SYNC*.TXT' -File).Count) {throw "Engine reported a synchronization error: $dir"}
        }
    } while($active.Count -and $watch.Elapsed.TotalSeconds -lt $TimeoutSeconds)
    if($active.Count) {throw "Multiplayer timed out after $TimeoutSeconds seconds: $run"}
    foreach($dir in $dirs) {
        if((Get-Content -LiteralPath (Join-Path $dir 'multiplayer.log') -Raw) -notmatch 'PASS completed network run') {throw "Incomplete multiplayer run: $dir"}
        if($ActSp) {
            $positions=@(Get-ChildItem -LiteralPath (Join-Path $dir 'samples') -Filter '*.txt' | Select-String -Pattern '^T \d+ HORNET \d+ \d+ 1 0 (\d+ \d+) ' | ForEach-Object {$_.Matches[0].Groups[1].Value} | Sort-Object -Unique)
            if($positions.Count -lt 3) {throw "Expected deployed drones moving between orbit stations in $dir"}
        }
    }
    $identities=@($dirs | ForEach-Object {
        $log=Get-Content -LiteralPath (Join-Path $_ 'multiplayer.log') -Raw
        if($log -notmatch "LOCAL house=(\d+) mode=\d+ players=$PlayerCount") {throw 'Missing real multiplayer identity'}
        $Matches[1]
    })
    if(@($identities | Select-Object -Unique).Count -ne $PlayerCount) {throw 'Both peers are controlling the same player'}
    $a=@(Get-Content -LiteralPath (Join-Path $dirs[0] 'snapshots.csv'))
    $expected=if($MountsOnly){601} elseif($PlayerCount -eq 3) {if($NetworkProfile -eq 'Direct'){966}else{1221}} elseif($NetworkProfile -eq 'Direct'){881}else{961}
    Compare-Samples
    if($a.Count -ne $expected -or $compared -ne $expected) {throw 'Missing snapshot frames'}
    for($i=0;$i -lt $a.Count;$i++) {if(($a[$i] -split ',')[0] -ne [string](20+$i)) {throw 'Non-contiguous snapshot frames'}}
    foreach($dir in $dirs) {
        if(@(Get-Content -LiteralPath (Join-Path $dir 'snapshots.csv')).Count -ne $expected) {throw "Incomplete samples: $dir"}
    }
    if($NetworkProfile -ne 'Direct') {
        $relayStats=Get-Content (Join-Path $run 'relay.json') -Raw | ConvertFrom-Json
        if(!$relayStats.dropped -or !$relayStats.blackoutDropped -or @($relayStats.directions | Where-Object {$_ -le 0}).Count -or $relayStats.directions.Count -ne $PlayerCount*($PlayerCount-1)) {throw 'Network impairment was not exercised in both directions'}
    }
    [pscustomobject]@{Result='PASS';MountsOnly=[bool]$MountsOnly;Players=$PlayerCount;Seed=$Seed;Houses=$identities;NetworkProfile=$NetworkProfile;Samples=$a.Count;Run=$run;Seconds=[int]$watch.Elapsed.TotalSeconds} | ConvertTo-Json | Set-Content (Join-Path $run 'result.json')
    Write-Output "PASS $PlayerCount real peers, $($a.Count) matching state samples: $run"
} catch {
    [pscustomobject]@{Result='FAIL';NetworkProfile=$NetworkProfile;ComparedSamples=$compared;Run=$run;Reason=$_.Exception.Message} | ConvertTo-Json | Set-Content (Join-Path $run 'result.json')
    throw
} finally {
    if($ReportPath -and (Test-Path -LiteralPath (Join-Path $run 'result.json'))) {Copy-Item -LiteralPath (Join-Path $run 'result.json') -Destination $ReportPath -Force}
    $paths=@($dirs | ForEach-Object {Join-Path $_ 'gamemd.exe';Join-Path $_ 'Syringe.exe'})
    Get-Process -Name gamemd,Syringe -ErrorAction SilentlyContinue | Where-Object {$_.Path -in $paths} | Stop-Process -Force
    if($relay -and !$relay.HasExited) {
        $null=New-Item -ItemType File -Force (Join-Path $run 'relay.stop')
        if(!$relay.WaitForExit(3000)) {$relay.Kill()}
    }
    if(!$KeepGameAssets) {
        foreach($dir in $dirs) {
            Remove-TestArchives $dir
        }
    }
}
