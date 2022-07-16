--TEST--
GC is traced (PHP 8.0+)
--SKIPIF--
<?php
if (
    version_compare(PHP_VERSION, '8.0') < 0
) {
    die('skip this test is for PHP 8.0+ only');
}
?>
--ENV--
return <<<END
SPX_ENABLED=1
SPX_BUILTINS=1
SPX_METRICS=zgr,zgb,zgc
SPX_FP_FOCUS=zgb
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
  Distinct functions  :        5

  ZE GC runs          :        9
  ZE GC root buffer   :    10.0K
  ZE GC collected     :    90.0K

Flat profile:

 ZE GC runs          | ZE GC root buffer   | ZE GC collected     |
 Inc.     | Exc.     | Inc.     | *Exc.    | Inc.     | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------+----------+----------
        9 |        0 |    10.0K |   100.0K |    90.0K |        0 |    50.0K | f
        9 |        0 |    10.0K |        0 |    90.0K |        0 |        1 | %s/spx_%s.php
        0 |        0 |        0 |        0 |        0 |        0 |        1 | ::zend_compile_file
        0 |        0 |        0 |        0 |        0 |        0 |        1 | ::php_request_shutdown
        9 |        9 |   -90.0K |   -90.0K |    90.0K |    90.0K |        9 | ::gc_collect_cycles