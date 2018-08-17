--TEST--
Log hidden
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1"
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
Normal output