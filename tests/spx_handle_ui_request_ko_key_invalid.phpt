--TEST--
Handle UI request (invalid key)
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1"
spx.http_ui_assets_dir="{PWD}/../assets/web-ui"
log_errors=on
--FILE--
<?php

$_SERVER['REMOTE_ADDR'] = '127.0.0.1';
$_GET['SPX_KEY'] = 'dev0';
$_GET['SPX_UI_URI'] = '/data/metrics';
if (! spx_ui_handle_request()) {
    echo "No request to handle\n";
}

?>
--EXPECTF--
PHP Notice:  SPX: access not granted: server & client ("dev0") key mismatch in %s/tests/spx_handle_ui_request_ko_key_invalid.php on line 6

Notice: SPX: access not granted: server & client ("dev0") key mismatch in %s/tests/spx_handle_ui_request_ko_key_invalid.php on line 6
No request to handle