--TEST--
INI profiling parameters: no unnecessary access check (no related log expected)
--CGI--
--INI--
spx.debug=1
spx.data_dir="{PWD}/tmp_data_dir_ini_params_no_unnecessary_access_check"
spx.http_profiling_enabled=1
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="*"
log_errors=on
--FILE--
<?php
echo 'Normal output';
?>
--EXPECTHEADERS--
SPX-Debug-Profiling-Triggered: 1
--EXPECT--
Normal output
--CLEAN--
<?php

exec(sprintf('rm -rf %s', escapeshellarg(__DIR__ . '/tmp_data_dir_ini_params_no_unnecessary_access_check')));

?>
