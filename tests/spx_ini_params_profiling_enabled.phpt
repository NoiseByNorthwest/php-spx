--TEST--
INI profiling parameters: profiling enabled
--CGI--
--INI--
spx.debug=1
spx.data_dir="{PWD}/tmp_data_dir_ini_params_profiling_enabled"
spx.http_profiling_enabled=1
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

exec(sprintf('rm -rf %s', escapeshellarg(__DIR__ . '/tmp_data_dir_ini_params_profiling_enabled')));

?>
