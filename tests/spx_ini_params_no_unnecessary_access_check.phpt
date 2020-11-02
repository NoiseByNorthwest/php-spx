--TEST--
INI profiling parameters: no unnecessary access check (no related log expected)
--CGI--
--INI--
spx.debug=1
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
