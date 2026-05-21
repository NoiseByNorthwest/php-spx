--TEST--
"full" report data
--ENV--
return <<<END
SPX_ENABLED=1
SPX_BUILTINS=0
SPX_AUTO_START=0
SPX_REPORT=full
END;
--FILE--
<?php
function baz() {
}

function bar() {
  baz();
}

function foo() {
  for ($i = 0; $i < 3; $i++) {
    bar();
  }
}

spx_profiler_start();

foo();

$key = spx_profiler_stop();

$data_dir = ini_get('spx.data_dir');
$profileFile = "$data_dir/$key.txt.zst";
if (!file_exists($profileFile)) {
  // If the current system does not have the zstd dev package installed, then fall back to using the gzip version.
  $profileFile = "$data_dir/$key.txt.gz";
  $catCommand = 'zcat';
} else {
  $catCommand = 'zstdcat';
  if (getenv('PATH') === false) {
    // PATH not propagated (macOS or Linux with PHP 7.0-7.1)
    $candidates = [
      '/opt/homebrew/bin/zstdcat', // macOS Apple Silicon
      '/usr/local/bin/zstdcat',    // macOS Intel Homebrew
      '/usr/bin/zstdcat',          // Linux
    ];
    foreach ($candidates as $path) {
      if (is_executable($path)) {
        $catCommand = $path;
        break;
      }
    }
  }
}

echo shell_exec(sprintf('%s %s', escapeshellcmd($catCommand), escapeshellarg($profileFile)));

?>
--EXPECTF--
[events]
|0|a0|a0
11|1|%S|
b|2|%S|
6|3|%S|
-3|%S|%S
-2|%S|%S
b|2|%S|
6|3|%S|
-3|%S|%S
-2|%S|%S
b|2|%S|
6|3|%S|
-3|%S|%S
-2|%S|%S
-1|%S|%S
-0|%S|%S
[functions]
%s/spx_full_report_data.php:1:%s/spx_full_report_data.php
%s/tests/spx_full_report_data.php:9:foo
%s/spx_full_report_data.php:5:bar
%s/spx_full_report_data.php:2:baz
