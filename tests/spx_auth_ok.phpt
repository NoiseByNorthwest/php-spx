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
{"key": "wt","short_name": "Wall Time","name": "Wall Time","type": "time","releasable": 0}
,{"key": "ct","short_name": "CPU Time","name": "CPU Time","type": "time","releasable": 0}
,{"key": "it","short_name": "Idle Time","name": "Idle Time","type": "time","releasable": 0}
,{"key": "zm","short_name": "ZE memory","name": "Zend Engine memory usage","type": "memory","releasable": 1}
,{"key": "zgr","short_name": "ZE GC runs","name": "Zend Engine GC run count","type": "quantity","releasable": 0}
,{"key": "zgb","short_name": "ZE GC root buffer","name": "Zend Engine GC root buffer length","type": "quantity","releasable": 1}
,{"key": "zgc","short_name": "ZE GC collected","name": "Zend Engine GC collected cycle count","type": "quantity","releasable": 0}
,{"key": "zif","short_name": "ZE file count","name": "Zend Engine included file count","type": "quantity","releasable": 0}
,{"key": "zc","short_name": "ZE class count","name": "Zend Engine class count","type": "quantity","releasable": 0}
,{"key": "zf","short_name": "ZE func. count","name": "Zend Engine function count","type": "quantity","releasable": 0}
,{"key": "zo","short_name": "ZE object count","name": "Zend Engine object count","type": "quantity","releasable": 1}
,{"key": "ze","short_name": "ZE error count","name": "Zend Engine error count","type": "quantity","releasable": 0}
,{"key": "io","short_name": "I/O Bytes","name": "I/O Bytes (reads + writes)","type": "memory","releasable": 0}
,{"key": "ior","short_name": "I/O Read Bytes","name": "I/O Read Bytes","type": "memory","releasable": 0}
,{"key": "iow","short_name": "I/O Written Bytes","name": "I/O Written Bytes","type": "memory","releasable": 0}
]}