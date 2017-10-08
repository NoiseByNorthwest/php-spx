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
    for ($i = 0; $i < 15; $i++) {
        foo();
    }
}

error_reporting(0);

trigger_error('');
for ($i = 0; $i < 10; $i++) {
    bar();
}

?>
--EXPECTF--
*** SPX Report ***

Global stats:

  Called functions    :    38.6K
  Distinct functions  :        3

  ZE error count      :    38.6K

Flat profile:

 ZE error count      |
 Inc.     | *Exc.    | Called   | Function
----------+----------+----------+----------
    38.5K |    36.1K |    36.1K | 2@foo
    38.6K |     2.4K |     2.4K | 2@bar
    38.6K |        1 |        1 | %s/spx_013.php
