# mqtt-server 压力测试

## 测试机器

* AMD EPYC 7K62 48-Core @ 2.6 GHz

* CPU - 4核 内存 - 4GB

## 压测工具

* [emqtt-bench](https://github.com/emqx/emqtt-bench)

* [mqtt-benchmark](https://github.com/krylovsk/mqtt-benchmark)

## 性能对比

主要与 [emqx](https://github.com/emqx/emqx) 进行性能对比，由于 `emqtt-bench` 工具限制了请求速率，无法
很好的比较两者的差距，在 Connect Benchmark 和 Sub Benchmark 两者耗时相同，Pub Benchmark 下创建 100 个连接，每个连接以每秒 1000 条消息的发送速率下， `emqx` 的发布速率大概是 5w，`mqtt-server` 的发布速率大概是 6w,当
连接数较大时 `emqtt-bench` 所消耗内存较大，导致本机无法测试上万数量的并发连接，因此后续使用 `mqtt-benchmark` 进行测试，不过使用 `mqt--benchmark` 也只能测试最多 `C20k` 的样子。

### 1. 并发连接测试

主要测试并发连接的处理能力，因此不太关心负载的大小，以 `Qos0` 级别为主, 由于
没有订阅者不需要转发消息，消息级别的影响并不是很大。

* C10k

```bash
./mqtt-benchmark --broker tcp://127.0.0.1:1883 --count 1 --size 100 --clients 10000 --qos 0 --format text

# emqx
========= TOTAL (10000) =========
Total Ratio:                 1.000 (10000/10000)
Total Runtime (sec):         4.693
Average Runtime (sec):       1.601
Msg time min (ms):           0.008
Msg time max (ms):           154.863
Msg time mean mean (ms):     1.925
Msg time mean std (ms):      14.203
Average Bandwidth (msg/sec): 0.670
Total Bandwidth (msg/sec):   6704.350

# mqtt-server
========= TOTAL (10000) =========
Total Ratio:                 1.000 (10000/10000)
Total Runtime (sec):         3.234
Average Runtime (sec):       1.157
Msg time min (ms):           0.010
Msg time max (ms):           67.487
Msg time mean mean (ms):     0.185
Msg time mean std (ms):      2.405
Average Bandwidth (msg/sec): 0.869
Total Bandwidth (msg/sec):   8685.407
```

* C15k

```bash
./mqtt-benchmark --broker tcp://127.0.0.1:1883 --count 1 --size 100 --clients 15000 --qos 0 --format text

# emqx
========= TOTAL (15000) =========
Total Ratio:                 1.000 (15000/15000)
Total Runtime (sec):         13.919
Average Runtime (sec):       5.065
Msg time min (ms):           0.010
Msg time max (ms):           254.572
Msg time mean mean (ms):     1.048
Msg time mean std (ms):      12.544
Average Bandwidth (msg/sec): 0.352
Total Bandwidth (msg/sec):   5278.151

# mqtt-server
========= TOTAL (15000) =========
Total Ratio:                 1.000 (15000/15000)
Total Runtime (sec):         6.055
Average Runtime (sec):       1.232
Msg time min (ms):           0.010
Msg time max (ms):           54.297
Msg time mean mean (ms):     0.125
Msg time mean std (ms):      1.438
Average Bandwidth (msg/sec): 0.829
Total Bandwidth (msg/sec):   12433.813
```

* C20k

```bash
./mqtt-benchmark --broker tcp://127.0.0.1:1883 --count 1 --size 100 --clients 20000 --qos 0 --format text

# emqx 由于内存占用过高退出群聊

# mqtt-server
========= TOTAL (20000) =========
Total Ratio:                 1.000 (20000/20000)
Total Runtime (sec):         19.900
Average Runtime (sec):       2.109
Msg time min (ms):           0.010
Msg time max (ms):           1171.341
Msg time mean mean (ms):     0.345
Msg time mean std (ms):      12.389
Average Bandwidth (msg/sec): 0.668
Total Bandwidth (msg/sec):   13365.305
```

### 2. 负载测试

主要是测试服务器占用内存是否会随着发布消息的大小而膨胀, 因此连接数不是特别重要, 更看中服务器携带负载的能力, `mqtt-server` 使用智能指针来保证消息内容自动生命周期管理, 在发布消息时只会保存一份 payload，避免消息发布时拷贝消息导致的内存占用膨胀，由于 `emqx` 为拒绝较大的数据包，因此只对 `mqtt-server` 进行测试。

* 单包 1MB 负载, 发送总量 1000MB

```bash
./mqtt-benchmark --broker tcp://127.0.0.1:18833 --count 1 --size 1048576 --clients 1000 --qos 0 --format text

# mqtt-server 启动时内存占用 0.2% (约 8M)
# mqtt-server 峰值内存占用 18% (约 720M), 处理完后降低至 4% (约 160M)
# 处理完高负载后, 由于服务器哈希表容器不会自动缩容, 内存占用会较刚启动时高, 属于
# 正常情况, 这样在连续处理高负载请求时能够节省重复扩容的开销
```