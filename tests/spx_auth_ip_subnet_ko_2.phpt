--TEST--
Authentication: KO (invalid IP address)
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="10.0.0.0/0"
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
Notice: SPX: access not granted: "10.0.0.1" IP is not in white list ("10.0.0.0/0") in Unknown on line 0
Normal output