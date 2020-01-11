--TEST--
Auto start disabled, with nested spans
--ENV--
return <<<END
SPX_ENABLED=1
SPX_AUTO_START=0
SPX_METRICS=zo
SPX_BUILTINS=0
SPX_REPORT=trace
SPX_TRACE_FILE=/dev/stdout
SPX_TRACE_SAFE=1
END;
--FILE--
<?php
echo "Normal output\n";

$objects = [];

function foo() {
    global $objects;

    $objects[] = new stdClass();
    bar();
    $objects[] = new stdClass();

    spx_profiler_start();

    spx_profiler_stop();
}

function bar() {
    global $objects;

    $objects[] = new stdClass();
    time();

	spx_profiler_start();

    $objects[] = new stdClass();
    time();

    spx_profiler_stop();
}

spx_profiler_start();

foo();

?>
--EXPECTF--
Normal output
 ZE object count                |
 Cum.     | Inc.     | Exc.     | Depth    | Function
----------+----------+----------+----------+----------
        0 |        0 |        0 |        1 | +%s/spx_auto_start_004.php
        0 |        0 |        0 |        2 |  +foo
        1 |        0 |        0 |        3 |   +bar
        3 |        2 |        2 |        3 |   -bar
        4 |        4 |        2 |        2 |  -foo
        4 |        4 |        0 |        1 | -%s/spx_auto_start_004.php

SPX trace file: /dev/stdout