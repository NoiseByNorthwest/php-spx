--TEST--
INI profiling parameters: profiling disabled by default
--CGI--
--INI--
spx.debug=1
--FILE--
<?php
echo 'Normal output';
?>
--EXPECTHEADERS--
SPX-Debug-Profiling-Triggered: 0
--EXPECT--
Normal output
