--TEST--
UI: URI confinement
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1"
spx.http_ui_assets_dir="{PWD}/../assets/web-ui"
log_errors=on
--ENV--
return <<<END
REMOTE_ADDR=127.0.0.1
REQUEST_URI=/
END;
--GET--
SPX_KEY=dev&SPX_UI_URI=/js/../../../README.md
--FILE--
<?php
// noop
?>
--EXPECT--
File not found.
