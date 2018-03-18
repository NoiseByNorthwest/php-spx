--TEST--
Authentication: OK
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1"
--ENV--
return <<<END
REMOTE_ADDR=127.0.0.1
REQUEST_URI=/_spx/data/metrics
END;
--GET--
SPX_KEY=dev
--FILE--
<?php
echo 'Normal output';
?>
--EXPECT--
{"results": [
{"key": "wt","name": "Wall Time","type": "time","releasable": 0}
,{"key": "ct","name": "CPU Time","type": "time","releasable": 0}
,{"key": "it","name": "Idle Time","type": "time","releasable": 0}
,{"key": "zm","name": "ZE memory","type": "memory","releasable": 1}
,{"key": "zr","name": "ZE root buffer","type": "quantity","releasable": 1}
,{"key": "zo","name": "ZE object count","type": "quantity","releasable": 1}
,{"key": "ze","name": "ZE error count","type": "quantity","releasable": 0}
,{"key": "io","name": "I/O Bytes","type": "memory","releasable": 0}
,{"key": "ior","name": "I/O Read Bytes","type": "memory","releasable": 0}
,{"key": "iow","name": "I/O Written Bytes","type": "memory","releasable": 0}
]}