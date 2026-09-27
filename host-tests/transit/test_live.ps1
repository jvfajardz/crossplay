$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw "$PSScriptRoot\..\..\src\apps_local\transit\TransitActivity.cpp"

$specs = @(
  @('nl-OpenOV_NL:S:30009571', 30, '52', 'Zuid'),
  @('nl-OpenOV_NL:S:30007408', 30, '52', 'Noord'),
  @('nl-OpenOV_NL:S:30001314', 30, '37', 'Amstelstation'),
  @('nl-OpenOV_NL:S:30009018', 30, '37', 'Station Noord'),
  @('nl-OpenOV_stoparea:18008', 30, 'Sprinter', 'Amsterdam Sloterdijk'),
  @('nl-OpenOV_stoparea:18177', 90, 'Sprinter', 'Hoorn Kersenboogerd')
)

foreach ($spec in $specs) {
  $escapedStop = [regex]::Escape($spec[0])
  $escapedHeadsign = [regex]::Escape($spec[3])
  if ($source -notmatch "$escapedStop[^\r\n]+$escapedHeadsign") {
    throw "Firmware route table does not pair $($spec[0]) with $($spec[3])"
  }
  $url = "https://api.transitous.org/api/v1/stoptimes?stopId=$($spec[0])&n=$($spec[1])"
  $payload = Invoke-RestMethod -Uri $url -TimeoutSec 25
  $matches = @($payload.stopTimes | Where-Object {
    $_.routeShortName -ieq $spec[2] -and $_.headsign -ieq $spec[3]
  })
  if (-not $matches.Count) { throw "No live $($spec[2]) departures toward $($spec[3]) at $($spec[0])" }
  Write-Host "PASS live $($spec[2]) toward $($spec[3]) ($($matches.Count) departures)"
}

$ovapiSpecs = @(
  @('37402010', '306', 'Amsterdam Noord'),
  @('30001311', '306', 'Purmerend Overwhere'),
  @('30005029', '26', 'IJburg'),
  @('30008248', '26', 'Centraal Station')
)
foreach ($spec in $ovapiSpecs) {
  $payload = Invoke-RestMethod -Uri "http://v0.ovapi.nl/tpc/$($spec[0])/departures" -TimeoutSec 25
  $passes = @($payload.($spec[0]).Passes.PSObject.Properties.Value)
  $matches = @($passes | Where-Object {
    $_.LinePublicNumber -ieq $spec[1] -and $_.DestinationName50 -ieq $spec[2]
  })
  if (-not $matches.Count) { throw "No live $($spec[1]) departures toward $($spec[2]) at $($spec[0])" }
  Write-Host "PASS live $($spec[1]) toward $($spec[2]) ($($matches.Count) departures)"
}

Write-Host 'TRANSIT-LIVE-REGRESSION: green'
