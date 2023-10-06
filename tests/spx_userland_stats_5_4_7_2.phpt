--TEST--
Userland stats (PHP 5.4-7.2)
--SKIPIF--
<?php
if (
    version_compare(PHP_VERSION, '5.4') < 0
    || version_compare(PHP_VERSION, '7.3') >= 0
) {
    die('skip this test is for PHP 5.4 to 7.2 only');
}
?>
--ENV--
return <<<END
SPX_ENABLED=1
SPX_BUILTINS=1
SPX_METRICS=zuc,zuf,zuo
SPX_FP_FOCUS=zuo
END;
--FILE--
<?php

eval(<<<EOS
class foo {
  function __construct()
  {
    \$this->a = 0;
    \$this->b = 0;
  }
}
EOS
);

function bar()
{
    $a = 0;
    $b = 0;
}

function baz()
{
    $a = 0;
    $b = 0;
    $c = 0;
}

$a = 0;
$b = 0;
$c = 0;
$d = 0;

?>
--EXPECTF--
*** SPX Report ***

Global stats:

  Called functions    :        5
  Distinct functions  :        5

  ZE class count      :        1
  ZE func. count      :        3
  ZE opcodes count    :       20

Flat profile:

 ZE class count      | ZE func. count      | ZE opcodes count    |
 Inc.     | Exc.     | Inc.     | Exc.     | Inc.     | *Exc.    | Called   | Function
----------+----------+----------+----------+----------+----------+----------+----------
        0 |        0 |        2 |        2 |       14 |       14 |        1 | ::zend_compile_file
        1 |        1 |        1 |        1 |        6 |        6 |        1 | ::zend_compile_string
        1 |        0 |        1 |        0 |        6 |        0 |        1 | %s/spx_%s.php
        0 |        0 |        0 |        0 |        0 |        0 |        1 | %s/spx_%s.php(%d) : eval()'d code
        0 |        0 |        0 |        0 |        0 |        0 |        1 | ::php_request_shutdown