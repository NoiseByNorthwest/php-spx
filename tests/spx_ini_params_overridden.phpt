--TEST--
INI profiling parameters: overridden by query string
--CGI--
--INI--
spx.debug=1
spx.http_profiling_enabled=1
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1"
--ENV--
return <<<END
REMOTE_ADDR=127.0.0.1
REQUEST_URI=/
END;
--GET--
SPX_KEY=dev&SPX_ENABLED=0
--FILE--
<?php
echo 'Normal output';
?>
--EXPECTHEADERS--
SPX-Debug-Profiling-Triggered: 0
--EXPECT--
Normal output
