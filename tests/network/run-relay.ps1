param([Parameter(Mandatory)][int]$Port,[ValidateRange(2,3)][int]$PlayerCount=2,[ValidateSet('Impaired','ImpairedStartup')][string]$Profile='Impaired',[Parameter(Mandatory)][string]$Directory)
$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'UdpRelay.cs')
[StarfallUdpRelay]::Run($Port,$PlayerCount,$Directory,($Profile -eq 'ImpairedStartup'))
