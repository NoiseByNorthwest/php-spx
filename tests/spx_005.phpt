--TEST--
Explicitly disabled
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