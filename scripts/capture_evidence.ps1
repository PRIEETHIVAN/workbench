param(
  [string]$Executable = (Join-Path $PSScriptRoot '..\build-release\Launch.exe'),
  [int]$Port = 18765,
  [int]$CaseCount = 100,
  [int]$MeshSize = 48
)
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
$working = Split-Path $exe -Parent
$outDir = Join-Path $PSScriptRoot '..\results'
New-Item -ItemType Directory -Force $outDir | Out-Null
$env:WORKBENCH_PORT = [string]$Port
$env:WORKBENCH_NO_BROWSER = '1'
$launchStarted = Get-Date
$process = Start-Process -FilePath $exe -WorkingDirectory $working -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $outDir 'server_stdout.log') -RedirectStandardError (Join-Path $outDir 'server_stderr.log')
try {
  $base = "http://127.0.0.1:$Port"
  $ready = $false
  for ($i = 0; $i -lt 30; $i++) {
    try { $null = Invoke-RestMethod "$base/api/health"; $ready = $true; break } catch { Start-Sleep -Milliseconds 250 }
  }
  if (-not $ready) { throw 'Workbench API did not become ready; see results/server_stderr.log.' }
  $launchToReadyMs = [math]::Round(((Get-Date) - $launchStarted).TotalMilliseconds, 2)
  $index = Invoke-WebRequest -UseBasicParsing "$base/"
  $dataStl = Invoke-WebRequest -UseBasicParsing "$base/data/sample.stl"
  $dataCsv = Invoke-WebRequest -UseBasicParsing "$base/data/sample_temperatures.csv"
  $three = Invoke-WebRequest -UseBasicParsing "$base/assets/three-r134.min.js"
  $chart = Invoke-WebRequest -UseBasicParsing "$base/assets/chart-4.4.0.umd.min.js"
  if ($index.StatusCode -ne 200 -or $dataStl.StatusCode -ne 200 -or $dataCsv.StatusCode -ne 200 -or $three.StatusCode -ne 200 -or $chart.StatusCode -ne 200) {
    throw 'Static frontend, local libraries, or sample data did not return HTTP 200.'
  }

  $system = Invoke-RestMethod "$base/api/system/info"
  $deviceList = Invoke-RestMethod "$base/api/embedded/devices"
  $deviceBody = @{device_id=0} | ConvertTo-Json -Compress
  $deviceConnect = Invoke-RestMethod -Uri "$base/api/embedded/connect" -Method Post -ContentType 'application/json' -Body $deviceBody
  $telemetry = Invoke-RestMethod -Uri "$base/api/embedded/telemetry" -Method Post -ContentType 'application/json' -Body $deviceBody
  $jtagBody = @{device_id=2} | ConvertTo-Json -Compress
  $jtagFirst = Invoke-RestMethod -Uri "$base/api/embedded/connect" -Method Post -ContentType 'application/json' -Body $jtagBody
  $jtagRetry = Invoke-RestMethod -Uri "$base/api/embedded/connect" -Method Post -ContentType 'application/json' -Body $jtagBody
  $bpBody = @{w=0.5;b=0;v=1;c=0;lr=0.1;steps=100} | ConvertTo-Json -Compress
  $backprop = Invoke-RestMethod -Uri "$base/api/backprop/run" -Method Post -ContentType 'application/json' -Body $bpBody
  $fea = @()
  foreach ($n in @(10,20,40,80)) {
    $r = Invoke-RestMethod -Uri "$base/api/fea/solve" -Method Post -ContentType 'application/json' -Body (@{ n_elements=$n } | ConvertTo-Json -Compress)
    $fea += [pscustomobject]@{ n_elements=$n; max_abs_midpoint_error_C=$r.max_error }
  }
  $stlText = [System.IO.File]::ReadAllText((Join-Path $PSScriptRoot '..\data\sample.stl'))
  $csvText = [System.IO.File]::ReadAllText((Join-Path $PSScriptRoot '..\data\sample_temperatures.csv'))
  $stl = Invoke-RestMethod -Uri "$base/api/stl/parse" -Method Post -ContentType 'application/json' -Body (@{content=$stlText} | ConvertTo-Json -Compress)
  $csv = Invoke-RestMethod -Uri "$base/api/csv/parse" -Method Post -ContentType 'application/json' -Body (@{content=$csvText} | ConvertTo-Json -Compress)
  if (-not $stl.valid -or -not $csv.valid) { throw "Bundled sample parser validation failed: STL=$($stl.error_msg) CSV=$($csv.error_msg)" }
  $emptyStl = Invoke-RestMethod -Uri "$base/api/stl/parse" -Method Post -ContentType 'application/json' -Body (@{content=''} | ConvertTo-Json -Compress)
  $malformedStl = Invoke-RestMethod -Uri "$base/api/stl/parse" -Method Post -ContentType 'application/json' -Body (@{content='not an STL'} | ConvertTo-Json -Compress)
  $nonfiniteCsv = Invoke-RestMethod -Uri "$base/api/csv/parse" -Method Post -ContentType 'application/json' -Body (@{content="node_id,x_m,y_m,z_m,temperature_C`n1,0,0,0,NaN`n"} | ConvertTo-Json -Compress)
  foreach ($case in @($emptyStl,$malformedStl,$nonfiniteCsv)) {
    if ($case.valid -or [string]::IsNullOrWhiteSpace($case.error_msg)) {
      throw 'Invalid parser input was not rejected with a readable error.'
    }
  }

  $benchmarks = @()
  foreach ($workers in @(1,2,4)) {
    if ($workers -gt $system.logical_cores) { continue }
    $body = @{case_count=$CaseCount;mesh_size=$MeshSize;num_workers=$workers} | ConvertTo-Json -Compress
    $r = Invoke-RestMethod -Uri "$base/api/parallel/run" -Method Post -ContentType 'application/json' -Body $body
    if (-not $r.agreement_ok -or $r.max_serial_parallel_diff -gt 1e-8) { throw "Parallel numerical agreement failed for $workers workers." }
    $benchmarks += [pscustomobject]@{
      requested_workers=$workers; actual_workers=$r.worker_count; case_count=$r.case_count; mesh_baseline=$MeshSize; seed=42
      warmup_runs=$r.warmup_runs; measured_repetitions=$r.measured_repetitions
      median_serial_ms=$r.serial_wall_ms; median_parallel_ms=$r.parallel_wall_ms
      speedup=$r.speedup; efficiency=$r.efficiency; max_nodal_temperature_difference_C=$r.max_serial_parallel_diff
      agreement_ok=$r.agreement_ok; serial_errors=$r.serial_errors; parallel_errors=$r.parallel_errors
    }
  }
  $evidence = [pscustomobject]@{
    captured_at=(Get-Date).ToString('yyyy-MM-ddTHH:mm:ssK'); launch_to_ready_ms=$launchToReadyMs; system=$system
    device_count=$deviceList.count; mcu_connect_success=$deviceConnect.success; telemetry_success=$telemetry.success
    jtag_first_attempt_success=$jtagFirst.success; jtag_retry_success=$jtagRetry.success
    stl_triangles=$stl.triangle_count; csv_nodes=$csv.node_count; backprop=$backprop
    parser_error_cases=@(
      [pscustomobject]@{ test='empty STL'; rejected=(-not $emptyStl.valid); message=$emptyStl.error_msg }
      [pscustomobject]@{ test='malformed STL'; rejected=(-not $malformedStl.valid); message=$malformedStl.error_msg }
      [pscustomobject]@{ test='non-finite CSV'; rejected=(-not $nonfiniteCsv.valid); message=$nonfiniteCsv.error_msg }
    )
    fea=$fea; workload=@{case_count=$CaseCount;mesh_baseline=$MeshSize;seed=42}; benchmarks=$benchmarks
  }
  $evidence | ConvertTo-Json -Depth 20 | Set-Content -Encoding utf8 (Join-Path $outDir 'performance_raw.json')
  $benchmarks | Select-Object requested_workers,actual_workers,case_count,mesh_baseline,seed,warmup_runs,measured_repetitions,median_serial_ms,median_parallel_ms,speedup,efficiency,max_nodal_temperature_difference_C,agreement_ok | Export-Csv -NoTypeInformation -Encoding utf8 (Join-Path $outDir 'performance_summary.csv')
  $system | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 (Join-Path $outDir 'system_info.json')
  Copy-Item (Join-Path $working 'tests\results\fea_N*.csv') $outDir -Force
  Write-Output "Captured system, sample parser, FEA, and $($benchmarks.Count) performance rows into $outDir"
} finally {
  Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
  Remove-Item Env:WORKBENCH_PORT,Env:WORKBENCH_NO_BROWSER -ErrorAction SilentlyContinue
}
