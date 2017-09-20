--TEST--
Disabled by default
--ENV--
return <<<END
END;
--FILE--
<?php
echo 'Normal output';
?>
--EXPECT--
Normal output