--TEST--
Authentication: KO (invalid reverse proxy IPv6 address)
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_var="HTTP_X_FORWARDED_FOR"
spx.http_trusted_proxies="0000:0000:0000:0000:0000:ffff:127.0.0.2,0000:0000:0000:0000:0000:ffff:127.0.0.3,0000:0000:0000:0000:0000:ffff:127.0.0.4"
spx.http_ip_whitelist="0000:0000:0000:0000:0000:ffff:127.0.0.5"
log_errors=on
--ENV--
return <<<END
REMOTE_ADDR=0000:0000:0000:0000:0000:ffff:127.0.0.31
HTTP_X_FORWARDED_FOR=0000:0000:0000:0000:0000:ffff:127.0.0.5
REQUEST_URI=/
END;
--GET--
SPX_KEY=dev&SPX_UI_URI=/data/metrics
--FILE--
<?php
echo 'Normal output';
?>
--EXPECT--
Notice: SPX: access not granted: '0000:0000:0000:0000:0000:ffff:127.0.0.31' is not a trusted proxy in Unknown on line 0
Normal output