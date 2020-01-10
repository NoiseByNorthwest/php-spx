--TEST--
Auto start disabled
--ENV--
return <<<END
SPX_ENABLED=1
SPX_AUTO_START=0
END;
--FILE--
<?php
echo 'Normal output';
?>
--EXPECT--
Normal output