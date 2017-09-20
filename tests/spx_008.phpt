--TEST--
Specified output file
--ENV--
return <<<END
SPX_ENABLED=1
SPX_OUTPUT_FILE=tmp.txt
END;
--FILE--
<?php
echo 'Normal output';
?>
--CLEAN--
<?php
unlink('tmp.txt');
?>
--EXPECT--
Normal output