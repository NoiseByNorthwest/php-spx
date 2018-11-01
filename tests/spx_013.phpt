--TEST--
Flat profile: cycle depth
--ENV--
return <<<END
SPX_ENABLED=1
SPX_METRICS=ze
SPX_FP_FOCUS=ze
END;
--FILE--
<?php

function foo() {
    trigger_error('');

    if (count(debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS)) < 6) {
        bar();
    }
}

function bar() {
    trigger_error('');
    for ($i = 0; $i < 17; $i++) {
        foo();
    }
}

error_reporting(0);

trigger_error('');
for ($i = 0; $i < 13; $i++) {
    bar();
}

?>
--EXPECTF--
*** SPX Report ***

Global stats:

  Called functions    :    71.8K
  Distinct functions  :        3

  ZE error count      :    71.8K

Flat profile:

 ZE error count      |
 Inc.     | *Exc.    | Called   | Function
----------+----------+----------+----------
    71.8K |    67.8K |    67.8K | 2@foo
    71.8K |     4.0K |     4.0K | 2@bar
    71.8K |        1 |        1 | %s/spx_013.php
