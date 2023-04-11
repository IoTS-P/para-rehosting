函数 MQTTDeserialize_publish 的返回值异常导致访问非法地址访问

`MQTT_cycle` 函数在接收到 PUBLISH 类型的 mqtt 报文时，会调用 `MQTTDeserialize_publish` 函数解析报文内容。该函数中包含问题的部分如下：

```C
curdata += (rc = MQTTPacket_decodeBuf(curdata, &mylen));
enddata = curdata + mylen;

if (!readMQTTLenString(topicName, &curdata, enddata) || enddata - curdata < 0)
    goto exit;
```

调用 `MQTTPacket_decodeBuf` 并正常返回时，`rc` 值将被置 1。后续调用 `readMQTTLenString` 发生错误返回 0 时，`topicName` 中的 `lenstring` 将不被初始化，为任意地址，此时 `MQTTDeserialize_publish` 函数会错误返回 1 表示函数执行成功，使程序继续执行后续逻辑。

后续处理逻辑中访问该 `topicName` 中的字符串指针将会访问非法地址，导致程序崩溃。

一个可复现的导致崩溃的例子是（以 ASCII 字符串形式表示接收到的报文内容）
```
0000000000000000000000000000000000000000000000000000000000000000000000000000
```