--TEST--
GC is traced (PHP 7.2+)
--SKIPIF--
<?php
if (
    version_compare(PHP_VERSION, '7.2.0') < 0
) {
    die('skip this test is for PHP 7.2+ only');
}
?>
--ENV--
return <<<END
SPX_ENABLED=1
SPX_METRICS=zr
SPX_FP_FOCUS=zr
END;
--FILE--
<?php

function f() {
    $a = new stdClass;
    $b = new stdClass;

    $a->b = $b;
    $b->a = $a;
}

for ($i = 0; $i < 50 * 1000; $i++) {
    f();
}

?>
--EXPECTF--
*** SPX Report ***

Global stats:

  Called functions    :    50.0K
  Distinct functions  :        3

  ZE root buffer      :    10.0K

Flat profile:

 ZE root buffer      |
 Inc.     | *Exc.    | Called   | Function
----------+----------+----------+----------
    10.0K |   100.0K |    50.0K | f
    10.0K |        0 |        1 | %s/spx_%s.php
   -90000 |   -90000 |        9 | gc_collect_cycles