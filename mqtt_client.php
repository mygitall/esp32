<?php
/**
 * 极简 MQTT 3.1.1 客户端 — 纯 PHP，零依赖
 * 支持 CONNECT / SUBSCRIBE / 接收 PUBLISH
 */
class MQTTClient {
    private $socket;
    private $broker;
    private $port;
    private $clientId;
    private $keepAlive;
    private $lastPing = 0;

    public function __construct($broker, $port, $clientId, $keepAlive = 30) {
        $this->broker = $broker;
        $this->port = $port;
        $this->clientId = $clientId;
        $this->keepAlive = $keepAlive;
    }

    public function connect($timeout = 10) {
        $this->socket = @stream_socket_client(
            "tcp://{$this->broker}:{$this->port}",
            $errno, $errstr, $timeout,
            STREAM_CLIENT_CONNECT
        );
        if (!$this->socket) return false;

        stream_set_timeout($this->socket, 5);
        stream_set_blocking($this->socket, true);

        // MQTT CONNECT 报文
        $payload = pack('n', 4) . 'MQTT';                          // Protocol Name
        $payload .= chr(4);                                         // Protocol Level (MQTT 3.1.1)
        $payload .= chr(2);                                         // Connect Flags: Clean Session
        $payload .= pack('n', $this->keepAlive);                    // Keep Alive
        $payload .= $this->encodeStr($this->clientId);              // Client ID

        $this->send(0x10, $payload);

        // 读 CONNACK
        $resp = $this->readPacket();
        if (!$resp || $resp['type'] !== 0x20) return false;

        $this->lastPing = time();
        return true;
    }

    public function subscribe($topic, $qos = 0) {
        $packetId = rand(1, 65535);
        $payload = pack('n', $packetId);
        $payload .= $this->encodeStr($topic) . chr($qos);
        $this->send(0x82, $payload);
        // 读 SUBACK
        $resp = $this->readPacket();
        return $resp && $resp['type'] === 0x90;
    }

    /**
     * 读取一条 PUBLISH 消息（阻塞，带 keepalive）
     * 返回 ['topic' => ..., 'payload' => ...] 或 null
     */
    public function readMessage($timeoutSec = 60) {
        $deadline = time() + $timeoutSec;
        while (time() < $deadline) {
            // Keepalive: 如果快到时间了，发 PINGREQ
            if (time() - $this->lastPing >= $this->keepAlive - 5) {
                $this->send(0xC0, '');  // PINGREQ
                $this->lastPing = time();
            }

            $remaining = $deadline - time();
            $read = [$this->socket];
            $write = null; $except = null;
            $tv = max(0, min($remaining, 5));
            if (stream_select($read, $write, $except, $tv, 0) > 0) {
                $packet = $this->readPacket();
                if ($packet && $packet['type'] === 0x30) { // PUBLISH
                    $pos = 0;
                    $p = $packet['payload'];
                    $topicLen = unpack('n', substr($p, 0, 2))[1];
                    $topic = substr($p, 2, $topicLen);
                    $payload = substr($p, 2 + $topicLen);
                    return ['topic' => $topic, 'payload' => $payload];
                }
                if ($packet && $packet['type'] === 0xD0) { // PINGRESP
                    continue;
                }
            }

            // 检查超时
            $info = stream_get_meta_data($this->socket);
            if ($info['timed_out']) return null;
        }
        return null;
    }

    public function disconnect() {
        if ($this->socket) {
            $this->send(0xE0, '');
            fclose($this->socket);
            $this->socket = null;
        }
    }

    // ========== 私有方法 ==========

    private function send($type, $payload) {
        $length = strlen($payload);
        $header = chr($type);
        do {
            $byte = $length % 128;
            $length = intdiv($length, 128);
            if ($length > 0) $byte |= 0x80;
            $header .= chr($byte);
        } while ($length > 0);
        fwrite($this->socket, $header . $payload);
    }

    private function readPacket() {
        $byte = fread($this->socket, 1);
        if ($byte === false || strlen($byte) === 0) return null;
        $type = ord($byte) & 0xF0;

        $multiplier = 1;
        $length = 0;
        do {
            $digit = ord(fread($this->socket, 1));
            $length += ($digit & 127) * $multiplier;
            $multiplier *= 128;
            if ($multiplier > 128 * 128 * 128 * 128) return null;
        } while (($digit & 128) !== 0);

        $payload = '';
        while ($length > 0) {
            $chunk = fread($this->socket, min($length, 8192));
            if ($chunk === false || strlen($chunk) === 0) break;
            $payload .= $chunk;
            $length -= strlen($chunk);
        }
        return ['type' => $type, 'payload' => $payload];
    }

    private function encodeStr($str) {
        $len = strlen($str);
        return pack('n', $len) . $str;
    }
}
