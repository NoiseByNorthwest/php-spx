--TEST--
Custom metadata
--INI--
log_errors=on
--ENV--
return <<<END
SPX_ENABLED=1
SPX_AUTO_START=0
SPX_REPORT=full
END;
--FILE--
<?php
function foo() {
}

$metadataStrings = [
  serialize([
    'id' => 1
  ]),
  null,
  str_repeat('a', 4094) . 'bb',
  str_repeat('a', 4095) . 'bb',
  'foo',
];

foreach ($metadataStrings as $metadataString) {
    spx_profiler_start();
    if ($metadataString !== null) {
      spx_profiler_full_report_set_custom_metadata_str($metadataString);
    }

    foo();

    $key = spx_profiler_stop();
    echo file_get_contents('/tmp/spx/' . $key . '.json');
}

?>
--EXPECTF--
{
  "key": "spx-full-%s",
  "exec_ts": %d,
  "host_name": "%s",
  "process_pid": %d,
  "process_tid": %d,
  "process_pwd": "%S\/php-spx%S",
  "cli": 1,
  "cli_command_line": "%S\/php-spx%S\/tests\/spx_custom_metadata.php",
  "http_request_uri": "n\/a",
  "http_method": "GET",
  "http_host": "n\/a",
  "custom_metadata_str": "a:1:{s:2:\"id\";i:1;}",
  "wall_time_ms": %d,
  "peak_memory_usage": %d,
  "called_function_count": 2,
  "call_count": 2,
  "recorded_call_count": 2,
  "enabled_metrics": [
    "wt"
    ,"zm"
  ]
}
{
  "key": "spx-full-%s",
  "exec_ts": %d,
  "host_name": "%s",
  "process_pid": %d,
  "process_tid": %d,
  "process_pwd": "%S\/php-spx%S",
  "cli": 1,
  "cli_command_line": "%S\/php-spx%S\/tests\/spx_custom_metadata.php",
  "http_request_uri": "n\/a",
  "http_method": "GET",
  "http_host": "n\/a",
  "custom_metadata_str": null,
  "wall_time_ms": %d,
  "peak_memory_usage": %d,
  "called_function_count": 2,
  "call_count": 2,
  "recorded_call_count": 2,
  "enabled_metrics": [
    "wt"
    ,"zm"
  ]
}
{
  "key": "spx-full-%s",
  "exec_ts": %d,
  "host_name": "%s",
  "process_pid": %d,
  "process_tid": %d,
  "process_pwd": "%S\/php-spx%S",
  "cli": 1,
  "cli_command_line": "%S\/php-spx%S\/tests\/spx_custom_metadata.php",
  "http_request_uri": "n\/a",
  "http_method": "GET",
  "http_host": "n\/a",
  "custom_metadata_str": "%saabb",
  "wall_time_ms": %d,
  "peak_memory_usage": %d,
  "called_function_count": 2,
  "call_count": 2,
  "recorded_call_count": 2,
  "enabled_metrics": [
    "wt"
    ,"zm"
  ]
}
PHP Notice:  SPX: spx_profiler_full_report_set_custom_metadata_str(): too large $customMetadataStr string, it must not exceed 4KB in %S/php-spx%S/tests/spx_custom_metadata.php on line 18

Notice: SPX: spx_profiler_full_report_set_custom_metadata_str(): too large $customMetadataStr string, it must not exceed 4KB in %S/php-spx%S/tests/spx_custom_metadata.php on line 18
{
  "key": "spx-full-%s",
  "exec_ts": %d,
  "host_name": "%s",
  "process_pid": %d,
  "process_tid": %d,
  "process_pwd": "%S\/php-spx%S",
  "cli": 1,
  "cli_command_line": "%S\/php-spx%S\/tests\/spx_custom_metadata.php",
  "http_request_uri": "n\/a",
  "http_method": "GET",
  "http_host": "n\/a",
  "custom_metadata_str": null,
  "wall_time_ms": %d,
  "peak_memory_usage": %d,
  "called_function_count": 2,
  "call_count": 2,
  "recorded_call_count": 2,
  "enabled_metrics": [
    "wt"
    ,"zm"
  ]
}
{
  "key": "spx-full-%s",
  "exec_ts": %d,
  "host_name": "%s",
  "process_pid": %d,
  "process_tid": %d,
  "process_pwd": "%S\/php-spx%S",
  "cli": 1,
  "cli_command_line": "%S\/php-spx%S\/tests\/spx_custom_metadata.php",
  "http_request_uri": "n\/a",
  "http_method": "GET",
  "http_host": "n\/a",
  "custom_metadata_str": "foo",
  "wall_time_ms": %d,
  "peak_memory_usage": %d,
  "called_function_count": 2,
  "call_count": 2,
  "recorded_call_count": 2,
  "enabled_metrics": [
    "wt"
    ,"zm"
  ]
}