--TEST--
INI profiling parameters: no effect in CLI
--INI--
spx.http_profiling_enabled=1
--ENV--
return <<<END
SPX_ENABLED=0
END;
--FILE--
<?php
echo 'Normal output';
?>
--EXPECT--
Normal output