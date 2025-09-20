--TEST--
Handle UI request (missing IP address)
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1"
spx.http_ui_assets_dir="{PWD}/../assets/web-ui"
log_errors=on
--FILE--
<?php

$_GET['SPX_KEY'] = 'dev';
$_GET['SPX_UI_URI'] = '/data/metrics';
if (! spx_ui_handle_request()) {
    echo "No request to handle\n";
}

?>
--EXPECTF--
PHP Notice:  SPX: access not granted: $_SERVER["REMOTE_ADDR"] is empty in %s/tests/spx_handle_ui_request_ko_ip_missing.php on line 5

Notice: SPX: access not granted: $_SERVER["REMOTE_ADDR"] is empty in %s/tests/spx_handle_ui_request_ko_ip_missing.php on line 5
No request to handle