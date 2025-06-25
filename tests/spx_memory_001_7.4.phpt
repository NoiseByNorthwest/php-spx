--TEST--
Memory (PHP 7.4)
--SKIPIF--
<?php
if (
    version_compare(PHP_VERSION, '7.4') < 0 ||
        version_compare(PHP_VERSION, '8.0') >= 0
) {
    die('skip this test is for PHP 7.4 only');
}
?>
--ENV--
return <<<END
SPX_ENABLED=1
SPX_METRICS=zm
SPX_REPORT=trace
SPX_TRACE_FILE=/dev/stdout
SPX_TRACE_SAFE=1
END;
--FILE--
<?php

function f1()
{
}

$a = [];

function f2()
{
    global $a;
    f1();
    $a[] = str_pad('0', 1024);
    f1();
}

f2();

?>
--EXPECTF--
ZE memory usage                |
 Cum.     | Inc.     | Exc.     | Depth    | Line     | Function
----------+----------+----------+----------+----------+----------
       0B |       0B |       0B |        1 |        0 | +%s/tests/spx_memory_001_%s.php
       0B |       0B |       0B |        2 |       17 |  +f2
      32B |       0B |       0B |        3 |       12 |   +f1
      32B |       0B |       0B |        3 |        0 |   -f1
    1.6KB |       0B |       0B |        3 |       14 |   +f1
    1.6KB |       0B |       0B |        3 |        0 |   -f1
    1.6KB |    1.6KB |    1.6KB |        2 |        0 |  -f2
    1.6KB |    1.6KB |       0B |        1 |        0 | -%s/tests/spx_memory_001_%s.php

SPX trace file: /dev/stdout