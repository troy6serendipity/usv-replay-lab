# 输入、时间和报告契约

## 心跳记录 v1

CSV 以 `time_ms,event` 为首行。每个数据行采用 `<非负整数毫秒>,heartbeat`。允许只有表头的空轨迹。时间单调不减，同一时间的记录保持文件顺序。每个输入最多 100,000 条心跳，结束时间及字段中的毫秒值最多 86,400,000。使用 ASCII 字符和 LF 或 CRLF 换行。空白数据行、额外列、乱序时间、超出场景终点的记录按输入错误处理。

## 场景 v1

`.case` 由下列六个按顺序排列的 `key=value` 行组成。场景文件的输入轨迹由 CLI 参数提供，故可用同一份 CSV 比较健康和异常。

~~~text
schema_version=1
scenario_id=dropout
end_ms=1500
timeout_ms=300
drop_window_ms=600:1100
expected_events=timeout@800,recovered@1100
~~~

- `schema_version` 选择文件契约，当前值为 `1`。`scenario_id` 为 1 到 64 个小写字母、数字或下划线，并以小写字母开头。
- `end_ms` 是包含在模拟中的结束时刻。`timeout_ms` 是正整数。`drop_window_ms` 取 `none` 或左闭右开区间 `begin:end`，右端点最多为 `end_ms + 1`，且满足字段上限。
- `expected_events` 取 `none` 或时间非递减、逗号分隔的 `timeout@毫秒`、`recovered@毫秒`。检查完整、有序的事件列表，漏报、误报、时刻错误都会不匹配。

版本号同时使解析器能明确拒绝未来未支持的格式。后续如需记录协议帧或多设备事件，将另定义版本和转换方式。

## 虚拟时间

1. 回放从 `0 ms` 开始，监视器处于 `waiting`，首个截止时间为 `timeout_ms`。
2. 丢失窗口内的事件在交付前过滤并计数。交付的心跳将截止时间更新为 `time_ms + timeout_ms`。
3. 如下次心跳晚于当前截止时间，在截止时刻记录 `timeout`，然后在该心跳到达时记录 `recovered`。心跳恰好在截止时间到达时，先交付并更新截止时间。同一毫秒内的多条心跳按输入顺序计数。
4. 即使最后一条记录已处理，仍推进到 `end_ms`。如果未恢复且截止时间落在此区间内，记录一次超时，最终状态为 `timed_out`。没有心跳且结束时间早于截止时刻时，状态保持 `waiting`。

核心按有序心跳及其截止点推进，运行时间与事件数相关。实验中的毫秒数是**逻辑时间**。后续本地套接字集成测试应独立定义真实时钟容差。

## CLI 和报告 v1

~~~text
usv_replay_lab TRACE.csv SCENARIO.case [reference|ignore_timeout]
~~~

`reference` 是默认监视器。`ignore_timeout` 为故意漏报的教学对照，使用相同心跳交付和场景期望。

| 退出码 | 标准输出及含义 |
| --- | --- |
| `0` | JSON，`passed: true`，预期事件和观测事件一致。 |
| `1` | JSON，`passed: false`，已完成回放，事件不匹配。 |
| `2` | 输入、参数或 I/O 错误，诊断写入 stderr。输入错误无结果 JSON。 |

JSON 包含 `report_version`、场景与被测对象标识、终点与超时设置、交付和丢失计数、最终状态、期望和观测事件列表以及 `passed`。列表事件用 `time_ms` 和 `kind` 表示。最终状态有 `waiting`、`online`、`timed_out`。报告不记录运行时的日期或机器路径，同一版本和输入可比较输出。
