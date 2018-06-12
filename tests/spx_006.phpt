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

  Wall time           :  %s
  ZE memory usage     :  %s

Flat profile:

 Wall time           | ZE memory usage     |
 Inc.     | *Exc.    | Inc.     | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------
 %s | %s | %s | %s |        1 | %s/spx_006.php
