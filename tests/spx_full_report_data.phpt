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

$zstdcatCommand = 'zstdcat';
if (getenv('PATH') === false) {
  // it happens with macos & PHP 7.0-7.1
  $zstdcatCommand = '/opt/homebrew/bin/' . $zstdcatCommand;
}

echo shell_exec("$zstdcatCommand /tmp/spx/$key.txt.zst");

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