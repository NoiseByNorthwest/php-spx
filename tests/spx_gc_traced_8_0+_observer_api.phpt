--TEST--
GC is traced (PHP 8.0+) and instrumentation through observer API
--SKIPIF--
<?php
if (
    version_compare(PHP_VERSION, '8.0') < 0 || version_compare(PHP_VERSION, '8.2') >= 0
) {
    die('skip this test is for PHP 8.0-8.1 only');
}
?>
--INI--
spx.use_observer_api=1
--ENV--
return <<<END
SPX_ENABLED=1
SPX_BUILTINS=1
SPX_METRICS=wt,zgr,zgb,zgc
SPX_FP_FOCUS=wt
SPX_FP_INC=1
END;
--FILE--
<?php

function f1() {
    $a = new stdClass;
    $b = new stdClass;

    $a->b = $b;
    $b->a = $a;

    for ($i = 0; $i < 100; $i++) {
        // spin the CPU to ensure that its cost is > gc_collect_cycles
    }
}

function f2() {
    f1();
}

for ($i = 0; $i < 50 * 1000; $i++) {
    f2();
}

?>
--EXPECTF--
*** SPX Report ***

Global stats:

  Called functions    :   100.0K
  Distinct functions  :        6

  Wall time           :   %s
  ZE GC runs          :        9
  ZE GC root buffer   :    10.0K
  ZE GC collected     :    90.0K

Flat profile:

 Wall time           | ZE GC runs          | ZE GC root buffer   | ZE GC collected     |
 *Inc.    | Exc.     | Inc.     | Exc.     | Inc.     | Exc.     | Inc.     | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------+----------+----------+----------+----------
 %w%s | %w%s |        9 |        0 |    10.0K |        0 |    90.0K |        0 |        1 | %s/tests/spx_gc_traced_8_0+_observer_api.php
 %w%s | %w%s |        9 |        0 |    10.0K |        0 |    90.0K |        0 |    50.0K | f2
 %w%s | %w%s |        9 |        0 |    10.0K |   100.0K |    90.0K |        0 |    50.0K | f1
 %w%s | %w%s |        9 |        9 |   -90.0K |   -90.0K |    90.0K |    90.0K |        9 | ::gc_collect_cycles
 %w%s | %w%s |        0 |        0 |        0 |        0 |        0 |        0 |        1 | ::zend_compile_file
 %w%s | %w%s |        0 |        0 |        0 |        0 |        0 |        0 |        1 | ::php_request_shutdown
