$ErrorActionPreference = 'Stop'
$activity = Get-Content -Raw "$PSScriptRoot\..\..\src\apps_local\habits\HabitsActivity.cpp"
$header = Get-Content -Raw "$PSScriptRoot\..\..\src\apps_local\habits\HabitsActivity.h"
$model = Get-Content -Raw "$PSScriptRoot\..\..\src\apps_local\habits\HabitModel.h"
$shelf = Get-Content -Raw "$PSScriptRoot\..\..\src\apps_local\Shelf.cpp"

$checks = [ordered]@{
  'archived view exists' = $header -match 'Archived'
  'active state has a public query' = $model -match 'bool isActive\(Id habitId, int day\) const'
  'historical schedule has a public query' = $model -match 'uint8_t scheduleMask\(Id habitId, int day\) const'
  'calendar arrows have matching touch zones' = $activity -match 'x < 150' -and $activity -match 'getScreenWidth\(\) - 150'
  'detail buttons scroll tasks' = $activity -match 'View::Detail\)[\s\r\n]+taskRow_ \+= direction'
  'detail supports vertical task swipe' = $activity -match 'SwipeDir::Up' -and $activity -match 'SwipeDir::Down'
  'date menu offers task list' = $activity -match '"TASK LIST"'
  'statistics draw monthly bars' = $activity -match 'MONTHLY COMPLETION' -and $activity -match 'barHeight'
  'task rows use the compact button font' = $activity -match 'drawTextFit\(toybox::kButtonFontId[^\r\n]+task\.name'
  'completed day is a solid mark' = $activity -match 'DayState::Complete\) renderer\.fillRoundedRect\(x, y, size, size'
  'habits use a dedicated icon' = $shelf -match '"HABITS", &icon_habits_32'
}

$failed = @($checks.GetEnumerator() | Where-Object { -not $_.Value })
foreach ($check in $checks.GetEnumerator()) {
  Write-Host ("{0} {1}" -f ($(if ($check.Value) { 'PASS' } else { 'FAIL' })), $check.Key)
}
if ($failed.Count) { exit 1 }
