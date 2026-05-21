--TEST--
UI: delete foo report
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

file_put_contents("$dataDir/foo.json", 'foo');
file_put_contents("$dataDir/foo.txt.gz", 'foo');
file_put_contents("$dataDir/foo.txt.zst", 'foo');

$_SERVER['REMOTE_ADDR'] = '127.0.0.1';
$_GET['SPX_KEY'] = 'dev';
$_GET['SPX_UI_URI'] = '/data/reports/delete/foo';
spx_ui_handle_request();

echo "Remaining files:\n", implode("\n", glob("$dataDir/foo.*"));
$clean();
?>
--EXPECT--
{"success": true}
Remaining files:
