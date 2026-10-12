import cn.dooraccess.app.CommandTracker;
import cn.dooraccess.app.DoorProtocol;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class ProtocolTests {
    private static int checks;
    private static void check(boolean condition, String message) {
        checks++;
        if (!condition) throw new AssertionError(message);
    }
    private static void invalid(Runnable action, String message) {
        boolean rejected = false;
        try { action.run(); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, message);
    }
    private static byte[] bytes(String text) { return text.getBytes(StandardCharsets.UTF_8); }
    private static final class Collector implements DoorProtocol.FrameListener {
        final List<String> lines = new ArrayList<>();
        int overflows;
        public void onLine(String line) { lines.add(line); }
        public void onOversizedFrame() { overflows++; }
    }
    public static void main(String[] args) {
        DoorProtocol.Config config = DoorProtocol.Config.defaults();
        check(Arrays.equals(config.payload(DoorProtocol.Operation.UNLOCK), bytes("UNLOCK\r\n")), "unlock bytes");
        check(Arrays.equals(config.payload(DoorProtocol.Operation.LOCK), bytes("LOCK\r\n")), "lock bytes");
        check(Arrays.equals(config.payload(DoorProtocol.Operation.QUERY), bytes("STATUS\r\n")), "query bytes");
        check(Arrays.equals(config.exitPayload(), new byte[] {0x65,0x78,0x69,0x74,0x54,0x6f,0x4d,0x65,0x6d,0x75,0x0d,0x0a}), "exitToMemu exact case and spelling with CRLF");
        check(config.parse("STATE:UNLOCKED") == DoorProtocol.Reply.UNLOCKED, "unlocked exact match");
        check(config.parse("STATE:LOCKED") == DoorProtocol.Reply.LOCKED, "locked exact match");
        for (String text : new String[] {"STATE:LOCKEDjunk", " STATE:LOCKED", "state:locked", "UNLOCKED", "AT+VERSION", "OK STATE:LOCKED"})
            check(config.parse(text) == DoorProtocol.Reply.OTHER, "must not guess state: " + text);
        check(config.parse("ERR:DENIED") == DoorProtocol.Reply.ERROR, "error classification");
        String wire = "STATE:UNLOCKED\r\nSTATE:LOCKED\nERR:权限不足\r";
        byte[] stream = bytes(wire);
        List<String> expected = Arrays.asList("STATE:UNLOCKED", "STATE:LOCKED", "ERR:权限不足");
        for (int boundary = 0; boundary <= stream.length; boundary++) {
            Collector listener = new Collector();
            DoorProtocol.FrameDecoder decoder = new DoorProtocol.FrameDecoder(listener);
            decoder.feed(Arrays.copyOfRange(stream, 0, boundary));
            decoder.feed(Arrays.copyOfRange(stream, boundary, stream.length));
            check(listener.lines.equals(expected), "split stream boundary " + boundary);
        }
        Collector oneByte = new Collector();
        DoorProtocol.FrameDecoder single = new DoorProtocol.FrameDecoder(oneByte);
        for (byte item : stream) single.feed(new byte[] {item});
        check(oneByte.lines.equals(expected), "single-byte reception, split CRLF and UTF-8");
        Collector partial = new Collector();
        DoorProtocol.FrameDecoder framing = new DoorProtocol.FrameDecoder(partial);
        framing.feed(bytes("STATE:LOCKED"));
        check(partial.lines.isEmpty(), "unterminated data cannot confirm a state");
        framing.reset(); framing.feed(bytes("\r\n"));
        check(partial.lines.isEmpty(), "disconnect/config reset discards partial frame");
        byte[] giant = new byte[DoorProtocol.MAX_FRAME_BYTES + 100];
        Arrays.fill(giant, (byte) 'x');
        framing.feed(giant); framing.feed(bytes("STATE:LOCKED\nSTATE:UNLOCKED\n"));
        check(partial.overflows == 1, "oversized frame detected once");
        check(partial.lines.equals(Arrays.asList("STATE:UNLOCKED")), "discard until delimiter, then recover");
        framing.reset(); partial.lines.clear();
        byte[] exact = new byte[DoorProtocol.MAX_FRAME_BYTES]; Arrays.fill(exact, (byte) 'a');
        framing.feed(exact); framing.feed(bytes("\n"));
        check(partial.lines.size() == 1 && partial.lines.get(0).length() == exact.length, "maximum frame accepted");
        check(Arrays.equals(DoorProtocol.hexPayload("00 ff\t0D\n0A"), new byte[] {0, (byte)255, 13, 10}), "HEX preserves null and high bytes");
        invalid(() -> DoorProtocol.hexPayload("0"), "odd HEX rejected");
        invalid(() -> DoorProtocol.hexPayload("0x01"), "prefix rejected");
        invalid(() -> DoorProtocol.hexPayload("GG"), "invalid HEX rejected");
        invalid(() -> DoorProtocol.hexPayload("  "), "empty HEX rejected");
        invalid(() -> DoorProtocol.textPayload("a".repeat(513), 3), "send byte limit");
        invalid(() -> DoorProtocol.textPayload("中".repeat(200), 3), "UTF-8 byte limit");
        invalid(() -> new DoorProtocol.Config("A", "A", "Q", 0, "U", "L"), "duplicate command rejected");
        invalid(() -> new DoorProtocol.Config("A\nB", "L", "Q", 0, "U", "L"), "multiline command rejected");
        invalid(() -> new DoorProtocol.Config("U", "L", "Q", 0, "S", "S"), "duplicate replies rejected");
        invalid(() -> new DoorProtocol.Config("U", "L", "Q", 4, "U", "L"), "ending out of range rejected");
        DoorProtocol.Config custom = new DoorProtocol.Config("O", "C", "?", 1, "OPEN", "CLOSED");
        check(Arrays.equals(custom.payload(DoorProtocol.Operation.LOCK), bytes("C\n")), "custom command and ending");
        check(Arrays.equals(custom.exitPayload(), bytes("exitToMemu\n")), "exit uses configured ending");
        check(custom.parse("OPEN") == DoorProtocol.Reply.UNLOCKED, "custom reply");
        check(Arrays.equals(DoorProtocol.textPayload("STATUS", 3), bytes("STATUS")), "no ending option");

        CommandTracker tracker = new CommandTracker();
        long first = tracker.begin(DoorProtocol.Operation.UNLOCK);
        check(tracker.isPending(), "write alone never completes request");
        check(tracker.receive(DoorProtocol.Reply.OTHER) == CommandTracker.Outcome.NONE && tracker.isPending(), "unrelated reply cannot confirm");
        check(tracker.receive(DoorProtocol.Reply.UNLOCKED) == CommandTracker.Outcome.SUCCESS, "matching unlock reply");
        long second = tracker.begin(DoorProtocol.Operation.LOCK);
        check(!tracker.timeout(first) && tracker.matches(second), "stale timer cannot cancel next request");
        check(tracker.receive(DoorProtocol.Reply.UNLOCKED) == CommandTracker.Outcome.WRONG_STATE, "wrong reported state isn't success");
        long third = tracker.begin(DoorProtocol.Operation.QUERY);
        check(tracker.receive(DoorProtocol.Reply.LOCKED) == CommandTracker.Outcome.SUCCESS, "query accepts locked state");
        tracker.begin(DoorProtocol.Operation.QUERY);
        check(tracker.receive(DoorProtocol.Reply.UNLOCKED) == CommandTracker.Outcome.SUCCESS, "query accepts unlocked state");
        tracker.begin(DoorProtocol.Operation.UNLOCK);
        check(tracker.receive(DoorProtocol.Reply.ERROR) == CommandTracker.Outcome.REJECTED, "error doesn't confirm unlock");
        long fourth = tracker.begin(DoorProtocol.Operation.LOCK);
        check(tracker.timeout(fourth) && !tracker.isPending(), "timeout releases request without success");
        tracker.begin(DoorProtocol.Operation.QUERY); tracker.clear();
        check(!tracker.isPending(), "disconnect cancels pending request");
        System.out.println("PASS: " + checks + " protocol and transaction checks");
    }
}
