--TEST--
GC is traced (PHP 7.0 - 7.1)
--SKIPIF--
<?php
if (
    version_compare(PHP_VERSION, '7.0.0') < 0
    || version_compare(PHP_VERSION, '7.2.0') >= 0
) {
    die('Skip: this test is for PHP 7.0 & 7.1 only');
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

  Wall Time           :  %s
  ZE root buffer      :  %s

Flat profile:

 Wall Time           | ZE root buffer      |
 Inc.     | *Exc.    | Inc.     | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------
 %s | %s |        0 |   100.0K |    50.0K | f
 %s | %s |        0 |        0 |        1 | %s/spx_gc_traced.php
 %s | %s |  -100000 |  -100000 |       10 | gc_collect_cycles