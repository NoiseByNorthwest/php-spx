--TEST--
Auto start disabled, builtins traced, trace report & several traced spans
--ENV--
return <<<END
SPX_ENABLED=1
SPX_AUTO_START=0
SPX_METRICS=zo
SPX_BUILTINS=1
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
}

function bar() {
    global $objects;

    $objects[] = new stdClass();
    time();

    spx_profiler_start();

    $objects[] = new stdClass();
    time();
}

for ($i = 0; $i < 3; $i++) {
    foo();
    $objects[] = new stdClass();
    spx_profiler_stop();
}

?>
--EXPECTF--
Normal output
 ZE object count                |
 Cum.     | Inc.     | Exc.     | Depth    | Function
----------+----------+----------+----------+----------
        0 |        0 |        0 |        1 | +%s/spx_auto_start_002.php
        0 |        0 |        0 |        2 |  +foo
        0 |        0 |        0 |        3 |   +bar
        0 |        0 |        0 |        4 |    +spx_profiler_start
        0 |        0 |        0 |        4 |    -spx_profiler_start
        1 |        0 |        0 |        4 |    +time
        1 |        0 |        0 |        4 |    -time
        1 |        1 |        1 |        3 |   -bar
        2 |        2 |        1 |        2 |  -foo
        3 |        0 |        0 |        2 |  +spx_profiler_stop
        3 |        0 |        0 |        2 |  -spx_profiler_stop
        3 |        3 |        1 |        1 | -%s/spx_auto_start_002.php

SPX trace file: /dev/stdout
 ZE object count                |
 Cum.     | Inc.     | Exc.     | Depth    | Function
----------+----------+----------+----------+----------
        0 |        0 |        0 |        1 | +%s/spx_auto_start_002.php
        0 |        0 |        0 |        2 |  +foo
        0 |        0 |        0 |        3 |   +bar
        0 |        0 |        0 |        4 |    +spx_profiler_start
        0 |        0 |        0 |        4 |    -spx_profiler_start
        1 |        0 |        0 |        4 |    +time
        1 |        0 |        0 |        4 |    -time
        1 |        1 |        1 |        3 |   -bar
        2 |        2 |        1 |        2 |  -foo
        3 |        0 |        0 |        2 |  +spx_profiler_stop
        3 |        0 |        0 |        2 |  -spx_profiler_stop
        3 |        3 |        1 |        1 | -%s/spx_auto_start_002.php

SPX trace file: /dev/stdout
 ZE object count                |
 Cum.     | Inc.     | Exc.     | Depth    | Function
----------+----------+----------+----------+----------
        0 |        0 |        0 |        1 | +%s/spx_auto_start_002.php
        0 |        0 |        0 |        2 |  +foo
        0 |        0 |        0 |        3 |   +bar
        0 |        0 |        0 |        4 |    +spx_profiler_start
        0 |        0 |        0 |        4 |    -spx_profiler_start
        1 |        0 |        0 |        4 |    +time
        1 |        0 |        0 |        4 |    -time
        1 |        1 |        1 |        3 |   -bar
        2 |        2 |        1 |        2 |  -foo
        3 |        0 |        0 |        2 |  +spx_profiler_stop
        3 |        0 |        0 |        2 |  -spx_profiler_stop
        3 |        3 |        1 |        1 | -%s/spx_auto_start_002.php

SPX trace file: /dev/stdout