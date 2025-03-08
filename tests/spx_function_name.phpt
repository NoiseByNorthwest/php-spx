--TEST--
Function name
--ENV--
return <<<END
SPX_ENABLED=1
SPX_FP_INC=1
END;
--FILE--
<?php

class C2
{
  public function m2()
  {
    usleep(1000);
  }
}

class C1
{
  public static function sm1()
  {
    $o = new C2();
    $f = function () use($o) { return $o->m2(); };

    usleep(1000);

    $f();
  }
}

function f()
{
  usleep(1000);

  (new class {
    public function m1()
    {
      usleep(1000);

      C1::sm1();
    }
  })->m1();
}

usleep(1000);

$f = function () {
  usleep(1000);
  f();
};

$f();
$f();

?>
--EXPECTF--
*** SPX Report ***

Global stats:

  Called functions    :       13
  Distinct functions  :        7

  Wall time           :   %s
  ZE memory usage     :   %s

Flat profile:

 Wall time           | ZE memory usage     |
 *Inc.    | Exc.     | Inc.     | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------
   %s |    %s |     %s |     %s |        1 | /var/www/php-spx/tests/spx_function_name.php
   %s |    %s |       %s |       %s |        2 | {closure:/var/www/php-spx/tests/spx_function_name.php:40}
    %s |    %s |       %s |       0B |        2 | f
    %s |    %s |       %s |       0B |        2 | class@anonymous::m1
    %s |    %s |       %s |       0B |        2 | C1::sm1
    %s |    %s |       %s |       0B |        2 | C1::{closure%S:16}
    %s |    %s |       %s |       %s |        2 | C2::m2