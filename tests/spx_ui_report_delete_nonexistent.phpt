--TEST--
UI: delete non-existent report
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1"
spx.data_dir="{PWD}/tmp_data_dir"
log_errors=on
--FILE--
<?php
$dataDir = ini_get('spx.data_dir');

$clean = function () use ($dataDir) { exec(sprintf('rm -rf %s', escapeshellarg($dataDir))); };
$clean();
@mkdir($dataDir);

$_SERVER['REMOTE_ADDR'] = '127.0.0.1';
$_GET['SPX_KEY'] = 'dev';
$_GET['SPX_UI_URI'] = '/data/reports/delete/foo';
spx_ui_handle_request();

$clean();
?>
--EXPECT--
File not found.
