--TEST--
Auto start disabled, explicit start with internal function as caller
--ENV--
return <<<END
SPX_ENABLED=1
SPX_AUTO_START=0
SPX_METRICS=zo
SPX_REPORT=trace
SPX_TRACE_FILE=/dev/stdout
END;
--FILE--
<?php
function foo() {
    bar();
}

function bar() {
    time();
}

array_map(
    function() {
        spx_profiler_start();
        foo();
        spx_profiler_stop();
    },
    [null]
);

?>
--EXPECT--
ZE object count                |
 Cum.     | Inc.     | Exc.     | Depth    | Line     | Function
----------+----------+----------+----------+----------+----------
        0 |        0 |        0 |        1 |        0 | +/var/www/php-spx/tests/spx_auto_start_008.php
        0 |        0 |        0 |        2 |       10 |  +{closure:/var/www/php-spx/tests/spx_auto_start_008.php:11}
        0 |        0 |        0 |        3 |       13 |   +foo
        0 |        0 |        0 |        4 |        3 |    +bar
        0 |        0 |        0 |        4 |        0 |    -bar
        0 |        0 |        0 |        3 |        0 |   -foo
        0 |        0 |        0 |        2 |        0 |  -{closure:/var/www/php-spx/tests/spx_auto_start_008.php:11}
        0 |        0 |        0 |        1 |        0 | -/var/www/php-spx/tests/spx_auto_start_008.php

SPX trace file: /dev/stdout
