--TEST--
Shutdown user function (PHP 8.3-)
--SKIPIF--
<?php
if (
    version_compare(PHP_VERSION, '8.4') >= 0
) {
    die('skip this test is for PHP 8.3- only');
}
?>
--ENV--
return <<<END
SPX_ENABLED=1
SPX_METRICS=zo
SPX_REPORT=trace
SPX_TRACE_FILE=/dev/stdout
SPX_TRACE_SAFE=1
END;
--FILE--
<?php

function f1()
{
    // noop
}

function f2()
{
    register_shutdown_function(function() {
        f1();
    });
}

f2();

?>
--EXPECTF--
ZE object count                |
 Cum.     | Inc.     | Exc.     | Depth    | Line     | Function
----------+----------+----------+----------+----------+----------
        0 |        0 |        0 |        1 |        0 | +%s/tests/spx_shutdown_func_%s.php
        0 |        0 |        0 |        2 |       15 |  +f2
        1 |        1 |        1 |        2 |        0 |  -f2
        1 |        1 |        0 |        1 |        0 | -%s/tests/spx_shutdown_func_%s.php
        1 |        0 |        0 |        1 |        0 | +{closure}
        1 |        0 |        0 |        2 |       11 |  +f1
        1 |        0 |        0 |        2 |        0 |  -f1
        1 |        0 |        0 |        1 |        0 | -{closure}

SPX trace file: /dev/stdout