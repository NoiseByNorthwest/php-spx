--TEST--
Auto start disabled, full report & span report keys printed
--CGI--
--INI--
spx.debug=1
spx.data_dir="{PWD}/tmp_data_dir_auto_start_007"
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1"
--ENV--
return <<<END
REMOTE_ADDR=127.0.0.1
REQUEST_URI=/
END;
--GET--
SPX_KEY=dev&SPX_ENABLED=1&SPX_AUTO_START=0
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

exec(sprintf('rm -rf %s', escapeshellarg(__DIR__ . '/tmp_data_dir_auto_start_007')));

?>
