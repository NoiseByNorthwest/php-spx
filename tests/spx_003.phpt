--TEST--
Authentication: KO (invalid key)
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1"
--ENV--
return <<<END
REMOTE_ADDR=127.0.0.1
END;
--GET--
SPX_KEY=foo&SPX_ENABLED=1
--FILE--
<?php
echo 'Normal output';
?>
--EXPECTF--
Normal output