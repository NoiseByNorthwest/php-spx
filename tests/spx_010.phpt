--TEST--
Flat profile
--ENV--
return <<<END
SPX_ENABLED=1
SPX_METRICS=zo,ze
END;
--FILE--
<?php

function foo() {
    global $objects;
    $o = new stdClass;
    $objects[] = $o;
    trigger_error('');

    usleep(100);
}

function bar() {
    global $objects;
    $o = new stdClass;
    $objects[] = $o;
    for ($i = 0; $i < 30; $i++) {
        foo();
    }

    usleep(100);
}

error_reporting(0);
$objects = [];
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

  Wall time           : %s
  ZE object count     :      310
  ZE error count      :      300

Flat profile:

 Wall time           | ZE object count     | ZE error count      |
 Inc.     | *Exc.    | Inc.     | Exc.     | Inc.     | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------+----------+----------
 %s | %s |      300 |      300 |      300 |      300 |      300 | foo
 %s | %s |      310 |       10 |      300 |        0 |       10 | bar
 %s | %s |        0 |     -310 |      300 |        0 |        1 | %s/spx_010.php
