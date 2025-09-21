--TEST--
Authentication: KO (invalid IP address)
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1,127.0.0.2,127.0.0.3"
spx.http_ui_assets_dir="{PWD}/../assets/web-ui"
log_errors=on
--ENV--
return <<<END
REMOTE_ADDR=127.0.0.21
REQUEST_URI=/
END;
--GET--
SPX_KEY=dev&SPX_UI_URI=/data/metrics
--FILE--
<?php
echo 'Normal output';
?>
--EXPECT--
Notice: SPX: access not granted: "127.0.0.21" IP is not in white list in Unknown on line 0
Normal output