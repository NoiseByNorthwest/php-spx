--TEST--
GC is traced (PHP 7.2+)
--SKIPIF--
<?php
if (
    version_compare(PHP_VERSION, '7.2.0') < 0
) {
    die('Skip: this test is for PHP 7.2+ only');
}
?>
--ENV--
return <<<END
SPX_ENABLED=1
SPX_METRICS=zr
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

  Wall Time           : %s
  ZE root buffer      : %s

Flat profile:

 Wall Time           | ZE root buffer      |
 Inc.     | *Exc.    | Inc.     | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------
 %s | %s |    10.0K |        0 |        1 | %s/spx_%s.php
 %s | %s |    10.0K |   100.0K |    50.0K | f
 %s | %s |   -90000 |   -90000 |        9 | gc_collect_cycles