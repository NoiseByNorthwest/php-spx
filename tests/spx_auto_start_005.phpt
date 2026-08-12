--TEST--
Auto start disabled, full report & span report keys printed
--INI--
spx.data_dir="{PWD}/tmp_data_dir_auto_start_005"
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
--CLEAN--
<?php

exec(sprintf('rm -rf %s', escapeshellarg(__DIR__ . '/tmp_data_dir_auto_start_005')));

?>
