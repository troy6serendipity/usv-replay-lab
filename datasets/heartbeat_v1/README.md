# heartbeat_v1 合成数据集

该数据集用代码生成，模拟一台无人艇的艇载电脑向岸站周期发送心跳。它包含 **16 条心跳**，从虚拟时间 **0 到 1500 毫秒**，间隔 **100 毫秒**。生成过程没有随机数。每条记录表示如果未丢失，该心跳会在对应时刻交给监控程序。

| 列 | 含义 | 例子 |
| --- | --- | --- |
| `time_ms` | 自回放起点起算的非负整数毫秒 | `500` |
| `event` | 此版事件类型，固定为 `heartbeat` | `heartbeat` |

`heartbeat.csv` 是两个场景共用的原始输入。`healthy.case` 交付全部心跳。`dropout.case` 丢弃 `[600, 1100)` 内的 5 条心跳，端点 1100 毫秒恢复交付。两者均在 1500 毫秒结束，超时阈值为 300 毫秒。

断链场景最后一次断链前交付在 500 毫秒，预期在 800 毫秒告警，并在 1100 毫秒恢复。两项预期记录在 `dropout.case`，由运行器与观察结果比较。

从项目根目录执行以下命令可检查或重新生成 CSV：

~~~text
python tools/generate_heartbeat.py --check
python tools/generate_heartbeat.py
~~~

格式详见[输入与报告契约](../../docs/formats.md)。实测报告的输入校验值见[实验摘要](../../evidence/step_01/summary.json)。
