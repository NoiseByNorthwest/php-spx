--TEST--
Live flat profile is not working if stdout is not a tty
--ENV--
return <<<END
SPX_ENABLED=1
SPX_FP_LIVE=1
END;
--FILE--
<?php
// if live flat profile is active, this output will not be printed to stdout
echo 'Normal output';
?>
--EXPECTF--
Normal output
*** SPX Report ***

Global stats:

  Called functions    :        1
  Distinct functions  :        1

  Wall Time           :    %s
  ZE memory           :    %s

Flat profile:

 Wall Time           | ZE memory           |
 Inc.     | *Exc.    | Inc.     | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------
 %s | %s | %s | %s |        1 | main
