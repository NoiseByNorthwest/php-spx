--TEST--
Authentication: KO (invalid key)
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1"
log_errors=on
--ENV--
return <<<END
REMOTE_ADDR=127.0.0.1
REQUEST_URI=/_spx/data/metrics
END;
--GET--
SPX_KEY=dev1
--FILE--
<?php
echo 'Normal output';
?>
--EXPECTF--
Notice: SPX: access not granted: server ("dev") & client ("dev1") key mismatch in Unknown on line 0
Normal output