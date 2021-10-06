--TEST--
Authentication: KO (invalid reverse proxy IP address)
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_var="HTTP_X_FORWARDED_FOR"
spx.http_trusted_proxies="127.0.0.2,127.0.0.3,127.0.0.4"
spx.http_ip_whitelist="127.0.0.5"
log_errors=on
--ENV--
return <<<END
REMOTE_ADDR=127.0.0.31
HTTP_X_FORWARDED_FOR=127.0.0.5
REQUEST_URI=/
END;
--GET--
SPX_KEY=dev&SPX_UI_URI=/data/metrics
--FILE--
<?php
echo 'Normal output';
?>
--EXPECT--
Notice: SPX: access not granted: '127.0.0.31' is not a trusted proxy in Unknown on line 0
Normal output