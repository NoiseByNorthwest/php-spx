--TEST--
Web UI: 404
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1"
--ENV--
return <<<END
REMOTE_ADDR=127.0.0.1
REQUEST_URI=/_spx/foo
END;
--GET--
SPX_KEY=dev
--FILE--
<?php
echo 'Normal output';
?>
--EXPECTHEADERS--
Status: 404 Not Found
--EXPECT--
File not found.