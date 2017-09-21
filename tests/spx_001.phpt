--TEST--
Authentication: OK
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
SPX_KEY=dev&SPX_ENABLED=1
--FILE--
<?php
echo 'Normal output';
?>
--EXPECTF--
*** SPX Report ***

Global stats:

  Called functions    :        1
  Distinct functions  :        1

  Wall Time           : %s
  ZE memory           : %s

Flat profile:

 Wall Time           | ZE memory           |
 Inc.     | *Exc.    | Inc.     | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------
     %s |     %s |     %s |     %s |        1 | %s/spx_001.php
