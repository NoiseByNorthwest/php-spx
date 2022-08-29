--TEST--
INI profiling parameters: profiling enabled
--CGI--
--INI--
spx.debug=1
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

exec("rm -rf /tmp/spx");

?>