--TEST--
Authentication: OK (valid IP address)
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="10.0.0.0/24"
spx.http_ui_assets_dir="{PWD}/../assets/web-ui"
log_errors=on
--ENV--
return <<<END
REMOTE_ADDR=10.0.0.1
REQUEST_URI=/
END;
--GET--
SPX_KEY=dev&SPX_UI_URI=/data/metrics
--FILE--
<?php
echo 'Normal output';
?>
--EXPECT--
{"results": [
{"key": "wt","short_name": "Wall time","name": "Wall time","type": "time","releasable": 0}
,{"key": "ct","short_name": "CPU time","name": "CPU time","type": "time","releasable": 0}
,{"key": "it","short_name": "Idle time","name": "Idle time","type": "time","releasable": 0}
,{"key": "zm","short_name": "ZE memory usage","name": "Zend Engine memory usage","type": "memory","releasable": 1}
,{"key": "zmac","short_name": "ZE alloc count","name": "Zend Engine allocation count","type": "quantity","releasable": 0}
,{"key": "zmab","short_name": "ZE alloc bytes","name": "Zend Engine allocated bytes","type": "memory","releasable": 0}
,{"key": "zmfc","short_name": "ZE free count","name": "Zend Engine free count","type": "quantity","releasable": 0}
,{"key": "zmfb","short_name": "ZE free bytes","name": "Zend Engine freed bytes","type": "memory","releasable": 0}
,{"key": "zgr","short_name": "ZE GC runs","name": "Zend Engine GC run count","type": "quantity","releasable": 0}
,{"key": "zgb","short_name": "ZE GC root buffer","name": "Zend Engine GC root buffer length","type": "quantity","releasable": 1}
,{"key": "zgc","short_name": "ZE GC collected","name": "Zend Engine GC collected cycle count","type": "quantity","releasable": 0}
,{"key": "zif","short_name": "ZE file count","name": "Zend Engine included file count","type": "quantity","releasable": 0}
,{"key": "zil","short_name": "ZE line count","name": "Zend Engine included line count","type": "quantity","releasable": 0}
,{"key": "zuc","short_name": "ZE class count","name": "Zend Engine user class count","type": "quantity","releasable": 0}
,{"key": "zuf","short_name": "ZE func. count","name": "Zend Engine user function count","type": "quantity","releasable": 0}
,{"key": "zuo","short_name": "ZE opcodes count","name": "Zend Engine user opcode count","type": "quantity","releasable": 0}
,{"key": "zo","short_name": "ZE object count","name": "Zend Engine object count","type": "quantity","releasable": 1}
,{"key": "ze","short_name": "ZE error count","name": "Zend Engine error count","type": "quantity","releasable": 0}
,{"key": "mor","short_name": "Own RSS","name": "Process's own RSS","type": "memory","releasable": 1}
,{"key": "io","short_name": "I/O Bytes","name": "I/O Bytes (reads + writes)","type": "memory","releasable": 0}
,{"key": "ior","short_name": "I/O Read Bytes","name": "I/O Read Bytes","type": "memory","releasable": 0}
,{"key": "iow","short_name": "I/O Written Bytes","name": "I/O Written Bytes","type": "memory","releasable": 0}
]}