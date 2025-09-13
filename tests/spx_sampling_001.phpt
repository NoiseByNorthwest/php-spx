--TEST--
Sampling mode
--SKIPIF--
<?php
if (
    stripos(php_uname(), 'Darwin') !== false
) {
    die('skip this test is too flaky on macOS');
}
?>
--ENV--
return <<<END
SPX_ENABLED=1
SPX_BUILTINS=0
SPX_SAMPLING_PERIOD=1
SPX_METRICS=wt
SPX_REPORT=trace
SPX_TRACE_FILE=/dev/stdout
SPX_TRACE_SAFE=1
END;
--FILE--
<?php

function f1()
{
    usleep(100 * 1000);
}

function f2()
{
    for ($i = 0; $i < 5; $i++) {
        array_map('f1', [null]);
    }
}

f2();

?>
--EXPECTF--
Wall time                      |
 Cum.     | Inc.     | Exc.     | Depth    | Line     | Function
----------+----------+----------+----------+----------+----------
      0ns |      0ns |      0ns |        1 |        0 | +%s/tests/spx_sampling_001.php
%w%f%cs |      0ns |      0ns |        2 |       15 |  +f2
%w%fus |      0ns |      0ns |        3 |       11 |   +f1
  10%fms |  10%fms |  10%fms |        3 |        0 |   -f1
  10%fms |      0ns |      0ns |        3 |       11 |   +f1
  20%fms |  10%fms |  10%fms |        3 |        0 |   -f1
  20%fms |      0ns |      0ns |        3 |       11 |   +f1
  30%fms |  10%fms |  10%fms |        3 |        0 |   -f1
  30%fms |      0ns |      0ns |        3 |       11 |   +f1
  40%fms |  10%fms |  10%fms |        3 |        0 |   -f1
  40%fms |      0ns |      0ns |        3 |       11 |   +f1
  50%fms |  10%fms |  10%fms |        3 |        0 |   -f1
  50%fms |  50%fms | %w%f%cs |        2 |        0 |  -f2
  50%fms |  50%fms | %w%fus |        1 |        0 | -%s/tests/spx_sampling_001.php

SPX trace file: /dev/stdout