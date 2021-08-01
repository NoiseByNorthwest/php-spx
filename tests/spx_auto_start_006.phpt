--TEST--
Auto start disabled, fp report & span report keys printed (null expected)
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
function foo() {
    bar();
}

function bar() {
    time();
}

for ($i = 0; $i < 3; $i++) {
    spx_profiler_start();
    foo();
    $key = spx_profiler_stop();
    echo "Report key: ", var_export($key, true), "\n";
}

?>
--EXPECTF--
ZE object count                |
 Cum.     | Inc.     | Exc.     | Depth    | Function
----------+----------+----------+----------+----------
        0 |        0 |        0 |        1 | +%s/spx_auto_start_006.php
        0 |        0 |        0 |        2 |  +foo
        0 |        0 |        0 |        3 |   +bar
        0 |        0 |        0 |        3 |   -bar
        0 |        0 |        0 |        2 |  -foo
        0 |        0 |        0 |        1 | -%s/spx_auto_start_006.php

SPX trace file: /dev/stdout
Report key: NULL
 ZE object count                |
 Cum.     | Inc.     | Exc.     | Depth    | Function
----------+----------+----------+----------+----------
        0 |        0 |        0 |        1 | +%s/spx_auto_start_006.php
        0 |        0 |        0 |        2 |  +foo
        0 |        0 |        0 |        3 |   +bar
        0 |        0 |        0 |        3 |   -bar
        0 |        0 |        0 |        2 |  -foo
        0 |        0 |        0 |        1 | -%s/spx_auto_start_006.php

SPX trace file: /dev/stdout
Report key: NULL
 ZE object count                |
 Cum.     | Inc.     | Exc.     | Depth    | Function
----------+----------+----------+----------+----------
        0 |        0 |        0 |        1 | +%s/spx_auto_start_006.php
        0 |        0 |        0 |        2 |  +foo
        0 |        0 |        0 |        3 |   +bar
        0 |        0 |        0 |        3 |   -bar
        0 |        0 |        0 |        2 |  -foo
        0 |        0 |        0 |        1 | -%s/spx_auto_start_006.php

SPX trace file: /dev/stdout
Report key: NULL