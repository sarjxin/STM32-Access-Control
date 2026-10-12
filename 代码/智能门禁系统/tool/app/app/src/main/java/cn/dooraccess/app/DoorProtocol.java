package cn.dooraccess.app;

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Locale;

/** Byte-stream framing and command encoding. No Android dependency, so it can be tested on the PC. */
public final class DoorProtocol {
    public static final int MAX_FRAME_BYTES = 1024;
    public static final int MAX_SEND_BYTES = 512;
    public static final String EXIT_COMMAND = "exitToMemu";
    public enum Operation { UNLOCK, LOCK, QUERY }
    public enum Reply { UNLOCKED, LOCKED, ERROR, OTHER }
    public static final String[] ENDINGS = {"\r\n", "\n", "\r", ""};

    public static final class Config {
        public final String unlock, lock, query, unlockedReply, lockedReply;
        public final int ending;
        public Config(String unlock, String lock, String query, int ending,
                      String unlockedReply, String lockedReply) {
            this.unlock = checked(unlock, "开锁指令");
            this.lock = checked(lock, "上锁指令");
            this.query = checked(query, "状态查询指令");
            if (this.unlock.equals(this.lock) || this.unlock.equals(this.query) || this.lock.equals(this.query)) {
                throw new IllegalArgumentException("开锁、上锁和查询指令需要各不相同");
            }
            if (ending < 0 || ending >= ENDINGS.length) throw new IllegalArgumentException("结束符无效");
            this.ending = ending;
            this.unlockedReply = checked(unlockedReply, "解锁状态回执");
            this.lockedReply = checked(lockedReply, "上锁状态回执");
            if (this.unlockedReply.equals(this.lockedReply)) throw new IllegalArgumentException("两种状态回执不能相同");
            if (this.unlockedReply.startsWith("ERR:") || this.lockedReply.startsWith("ERR:")) {
                throw new IllegalArgumentException("状态回执不能使用 ERR: 前缀");
            }
        }
        public static Config defaults() {
            return new Config("UNLOCK", "LOCK", "STATUS", 0, "STATE:UNLOCKED", "STATE:LOCKED");
        }
        private static String checked(String value, String label) {
            if (value == null || value.trim().isEmpty()) throw new IllegalArgumentException(label + "不能为空");
            String text = value.trim();
            if (text.length() > 128) throw new IllegalArgumentException(label + "最长 128 个字符");
            for (int i = 0; i < text.length(); i++) {
                if (Character.isISOControl(text.charAt(i))) throw new IllegalArgumentException(label + "不能包含换行或控制字符");
            }
            return text;
        }
        public String command(Operation operation) {
            switch (operation) {
                case UNLOCK: return unlock;
                case LOCK: return lock;
                default: return query;
            }
        }
        public byte[] payload(Operation operation) { return textPayload(command(operation), ending); }
        public byte[] exitPayload() { return textPayload(EXIT_COMMAND, ending); }
        public Reply parse(String line) {
            // Exact matching matters: STATE:UNLOCKED must never be recognized as LOCKED.
            if (unlockedReply.equals(line)) return Reply.UNLOCKED;
            if (lockedReply.equals(line)) return Reply.LOCKED;
            if (line.startsWith("ERR:")) return Reply.ERROR;
            return Reply.OTHER;
        }
    }

    public static byte[] textPayload(String text, int ending) {
        if (text == null || text.isEmpty()) throw new IllegalArgumentException("请输入待发送的数据");
        if (ending < 0 || ending >= ENDINGS.length) throw new IllegalArgumentException("结束符无效");
        byte[] bytes = (text + ENDINGS[ending]).getBytes(StandardCharsets.UTF_8);
        if (bytes.length > MAX_SEND_BYTES) throw new IllegalArgumentException("一次最多发送 512 字节");
        return bytes;
    }

    public static byte[] hexPayload(String text) {
        if (text == null) throw new IllegalArgumentException("请输入 HEX 数据");
        String hex = text.replaceAll("\\s", "");
        if (hex.isEmpty() || (hex.length() & 1) != 0) throw new IllegalArgumentException("HEX 每个字节需要两位，例如 01 0D 0A");
        if (hex.length() > MAX_SEND_BYTES * 2) throw new IllegalArgumentException("一次最多发送 512 字节");
        if (!hex.matches("[0-9a-fA-F]+")) throw new IllegalArgumentException("HEX 只接受 0–9、A–F 和空白，不需要 0x 前缀");
        byte[] result = new byte[hex.length() / 2];
        for (int i = 0; i < result.length; i++) result[i] = (byte) Integer.parseInt(hex.substring(i * 2, i * 2 + 2), 16);
        return result;
    }

    public static String hex(byte[] data, int maxBytes) {
        StringBuilder text = new StringBuilder();
        for (int i = 0; i < Math.min(data.length, maxBytes); i++) {
            if (i > 0) text.append(' ');
            text.append(String.format(Locale.ROOT, "%02X", data[i] & 255));
        }
        if (data.length > maxBytes) text.append(" … (共 ").append(data.length).append(" 字节)");
        return text.toString();
    }

    public interface FrameListener {
        void onLine(String line);
        void onOversizedFrame();
    }
    public static final class FrameDecoder {
        private final ByteArrayOutputStream buffer = new ByteArrayOutputStream();
        private final FrameListener listener;
        private boolean discarding;
        public FrameDecoder(FrameListener listener) { this.listener = listener; }
        public void reset() { buffer.reset(); discarding = false; }
        public void feed(byte[] data) {
            for (byte raw : data) {
                int value = raw & 255;
                if (value == '\r' || value == '\n') {
                    if (!discarding && buffer.size() > 0) {
                        String line = new String(buffer.toByteArray(), StandardCharsets.UTF_8);
                        buffer.reset();
                        listener.onLine(line);
                    }
                    buffer.reset();
                    discarding = false;
                } else if (!discarding) {
                    if (buffer.size() >= MAX_FRAME_BYTES) {
                        buffer.reset();
                        discarding = true;
                        listener.onOversizedFrame();
                    } else buffer.write(value);
                }
            }
        }
    }
}
