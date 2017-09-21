--TEST--
Flat profile: inc. sort
--ENV--
return <<<END
SPX_ENABLED=1
SPX_METRICS=zo,ze
SPX_FP_FOCUS=ze
SPX_FP_INC=1
END;
--FILE--
<?php

function foo() {
    global $objects;
    $o = new stdClass;
    $objects[] = $o;
    trigger_error('');
}

function bar() {
    global $objects;
    $o = new stdClass;
    $objects[] = $o;
    trigger_error('');
    for ($i = 0; $i < 30; $i++) {
        foo();
    }
}

error_reporting(0);
$objects = [];
trigger_error('');
for ($i = 0; $i < 10; $i++) {
    bar();
}

$objects = [];

?>
--EXPECTF--
*** SPX Report ***

Global stats:

  Called functions    :      311
  Distinct functions  :        3

  ZE object count     :      310
  ZE error count      :      311

Flat profile:

 ZE object count     | ZE error count      |
 Inc.     | Exc.     | *Inc.    | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------
        0 |     -310 |      311 |        1 |        1 | %s/spx_012.php
      310 |       10 |      310 |       10 |       10 | bar
      300 |      300 |      300 |      300 |      300 | foo
