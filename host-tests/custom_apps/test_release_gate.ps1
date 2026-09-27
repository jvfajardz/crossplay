[CmdletBinding()]
param([switch]$LiveTransit)

$ErrorActionPreference = 'Stop'

$repo = Resolve-Path "$PSScriptRoot\..\.."
$clang = 'C:\Program Files\LLVM\bin\clang++.exe'
if (-not (Test-Path -LiteralPath $clang -PathType Leaf)) {
  throw "LLVM clang++ is required for the custom-app regression gate: $clang"
}

$habits = Join-Path $repo 'src\apps_local\habits'
$testExe = Join-Path ([IO.Path]::GetTempPath()) "crossplay-habits-regression-$PID.exe"
try {
  & $clang -std=c++17 -O2 -Wall -Wextra -Werror `
    (Join-Path $habits 'HabitModel.cpp') `
    (Join-Path $habits 'HabitStats.cpp') `
    (Join-Path $habits 'HabitCsvStore.cpp') `
    (Join-Path $repo 'host-tests\habits\test_habits.cpp') `
    -o $testExe
  if ($LASTEXITCODE -ne 0) { throw "Habits host test compilation failed with exit code $LASTEXITCODE" }
  & $testExe
  if ($LASTEXITCODE -ne 0) { throw "Habits host tests failed with exit code $LASTEXITCODE" }
}
finally {
  Remove-Item -LiteralPath $testExe -Force -ErrorAction SilentlyContinue
}

& (Join-Path $repo 'host-tests\habits\test_ui_contract.ps1')
if ($LASTEXITCODE -ne 0) { throw "Habits UI contract failed with exit code $LASTEXITCODE" }

$transit = Get-Content -Raw (Join-Path $repo 'src\apps_local\transit\TransitActivity.cpp')
$shelf = Get-Content -Raw (Join-Path $repo 'src\apps_local\Shelf.cpp')
$checks = [ordered]@{
  '306 runs in both directions' = $transit -match '306: Koogsingel -> Noord' -and $transit -match '306: Noord -> Koogsingel'
  'M52 runs end to end in both directions' = $transit -match 'M52: Noord -> Zuid' -and $transit -match 'M52: Zuid -> Noord'
  '37 runs in both directions' = $transit -match '37: Noord -> Amstelstation' -and $transit -match '37: Amstelstation -> Noord'
  '37 outbound starts thirty minutes after the next 306' = $transit -match 'directions_\[0\]\.rows\[i\]\.expected \+ 30 \* 60'
  'Saturday selects T26' = $transit -match 'if \(saturday\(\)\)[\s\S]+fetchOvapi\(kTram26'
  'non-Saturday selects NS' = $transit -match 'else \{[\s\S]+fetchTransitous\(kNs'
  'live responses have a byte ceiling' = $transit -match 'kMaxResponseBytes = 256u \* 1024u'
  'Transit and Habits are both on the shelf' = $shelf -match '"TRANSIT"[\s\S]+"HABITS"'
  'Transit remains above Study' = $shelf -match '"TRANSIT"[\s\S]+"STUDY"'
}

$failed = @($checks.GetEnumerator() | Where-Object { -not $_.Value })
foreach ($check in $checks.GetEnumerator()) {
  Write-Host ("{0} {1}" -f ($(if ($check.Value) { 'PASS' } else { 'FAIL' })), $check.Key)
}
if ($failed.Count) { exit 1 }

if ($LiveTransit) {
  & (Join-Path $repo 'host-tests\transit\test_live.ps1')
  if ($LASTEXITCODE -ne 0) { throw "Transit live regression failed with exit code $LASTEXITCODE" }
}

Write-Host 'CUSTOM-APPS-REGRESSION: green'
