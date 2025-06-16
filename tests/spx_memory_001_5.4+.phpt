--TEST--
Memory (PHP 5)
--SKIPIF--
<?php
if (
    version_compare(PHP_VERSION, '5.4') < 0 ||
        version_compare(PHP_VERSION, '7.0') >= 0
) {
    die('skip this test is for PHP 5 only');
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
       0B |       0B |       0B |        1 |        0 | +%s/tests/spx_memory_001_5.4+.php
     288B |       0B |       0B |        2 |       17 |  +f2
     288B |       0B |       0B |        3 |       12 |   +f1
     288B |       0B |       0B |        3 |        0 |   -f1
    1.6KB |       0B |       0B |        3 |       14 |   +f1
    1.6KB |       0B |       0B |        3 |        0 |   -f1
    1.6KB |    1.2KB |    1.2KB |        2 |        0 |  -f2
    1.6KB |    1.6KB |     288B |        1 |        0 | -%s/tests/spx_memory_001_5.4+.php

SPX trace file: /dev/stdout