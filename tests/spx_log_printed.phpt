--TEST--
Log printed
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1"
log_errors=on
--ENV--
return <<<END
REMOTE_ADDR=127.0.0.1
REQUEST_URI=/
END;
--GET--
SPX_KEY=&SPX_UI_URI=/
--FILE--
<?php
echo 'Normal output';
?>
--EXPECT--
Notice: SPX: access not granted: client key is empty in Unknown on line 0
Normal output