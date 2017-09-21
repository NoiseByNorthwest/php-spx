--TEST--
Metrics selection
--ENV--
return <<<END
SPX_ENABLED=1
SPX_METRICS=foo,zr,bar,somethingverylong,it,ct,wt,zo,io,ior
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

  Wall Time           : %s
  CPU Time            : %s
  Idle Time           : %s
  ZE root buffer      : %s
  ZE object count     : %s
  I/O Bytes           : %s
  I/O Read Bytes      : %s

Flat profile:

 Wall Time           | CPU Time            | Idle Time           | ZE root buffer      | ZE object count     | I/O Bytes           | I/O Read Bytes      |
 Inc.     | *Exc.    | Inc.     | Exc.     | Inc.     | Exc.     | Inc.     | Exc.     | Inc.     | Exc.     | Inc.     | Exc.     | Inc.     | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------+----------+----------+----------+----------+----------+----------+----------+----------+----------+----------
 %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |        1 | %s/spx_007.php