--TEST--
Auto start disabled, full report & span report keys printed
--ENV--
return <<<END
SPX_ENABLED=1
SPX_AUTO_START=0
SPX_METRICS=zo
SPX_BUILTINS=0
SPX_REPORT=full
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
    echo spx_profiler_stop(), "\n";
}

?>
--EXPECTF--
spx-full-%s
spx-full-%s
spx-full-%s