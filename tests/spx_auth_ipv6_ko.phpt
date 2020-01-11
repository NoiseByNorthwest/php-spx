--TEST--
Authentication: KO (invalid IPv6 address)
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="0000:0000:0000:0000:0000:ffff:127.0.0.1,0000:0000:0000:0000:0000:ffff:127.0.0.2,0000:0000:0000:0000:0000:ffff:127.0.0.3"
log_errors=on
--ENV--
return <<<END
REMOTE_ADDR=0000:0000:0000:0000:0000:ffff:127.0.0.21
REQUEST_URI=/
END;
--GET--
SPX_KEY=dev&SPX_UI_URI=/data/metrics
--FILE--
<?php
echo 'Normal output';
?>
--EXPECT--
Notice: SPX: access not granted: "0000:0000:0000:0000:0000:ffff:127.0.0.21" IP is not in white list ("0000:0000:0000:0000:0000:ffff:127.0.0.1,0000:0000:0000:0000:0000:ffff:127.0.0.2,0000:0000:0000:0000:0000:ffff:127.0.0.3") in Unknown on line 0
Normal output