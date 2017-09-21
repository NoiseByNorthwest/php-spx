--TEST--
Enabled
--ENV--
return <<<END
SPX_ENABLED=1
END;
--FILE--
<?php
echo 'Normal output';
?>
--EXPECTF--
Normal output
*** SPX Report ***

Global stats:

  Called functions    :        1
  Distinct functions  :        1

  Wall Time           :   %s
  ZE memory           :   %s

Flat profile:

 Wall Time           | ZE memory           |
 Inc.     | *Exc.    | Inc.     | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------
 %s | %s | %s | %s |        1 | %s/spx_006.php
