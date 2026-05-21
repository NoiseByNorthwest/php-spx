--TEST--
An internal function calling a userland callback (PHP 8.2-8.3)
--SKIPIF--
<?php
if (
    version_compare(PHP_VERSION, '8.2') < 0
        || version_compare(PHP_VERSION, '8.4') >= 0
) {
    die('skip this test is for PHP 8.2-8.3 only');
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

function f2()
{
    array_map(
        function ($e) {
            return $e;
        },
        [1]
    );
}

f2();

?>
--EXPECTF--
ZE object count                |
 Cum.     | Inc.     | Exc.     | Depth    | Line     | Function
----------+----------+----------+----------+----------+----------
        0 |        0 |        0 |        1 |        0 | +%s/tests/spx_internal_calling_user_func_%s.php
        0 |        0 |        0 |        2 |       13 |  +f2
        1 |        0 |        0 |        3 |        5 |   +{closure}
        1 |        0 |        0 |        3 |        0 |   -{closure}
        0 |        0 |        0 |        2 |        0 |  -f2
        0 |        0 |        0 |        1 |        0 | -%s/tests/spx_internal_calling_user_func_%s.php

SPX trace file: /dev/stdout