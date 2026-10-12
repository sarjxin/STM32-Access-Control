package cn.dooraccess.app;

/** One foreground control request at a time; transport writes never confirm a lock state. */
public final class CommandTracker {
    public enum Outcome { NONE, SUCCESS, REJECTED, WRONG_STATE }
    private long sequence;
    private long pendingId;
    private DoorProtocol.Operation pending;
    public long begin(DoorProtocol.Operation operation) {
        if (pending != null) throw new IllegalStateException("上一条指令仍在等待回执");
        pending = operation;
        pendingId = ++sequence;
        return pendingId;
    }
    public boolean isPending() { return pending != null; }
    public boolean matches(long id) { return pending != null && pendingId == id; }
    public Outcome receive(DoorProtocol.Reply reply) {
        if (pending == null || reply == DoorProtocol.Reply.OTHER) return Outcome.NONE;
        Outcome outcome;
        if (reply == DoorProtocol.Reply.ERROR) outcome = Outcome.REJECTED;
        else if (pending == DoorProtocol.Operation.QUERY
            || (pending == DoorProtocol.Operation.LOCK && reply == DoorProtocol.Reply.LOCKED)
            || (pending == DoorProtocol.Operation.UNLOCK && reply == DoorProtocol.Reply.UNLOCKED)) outcome = Outcome.SUCCESS;
        else outcome = Outcome.WRONG_STATE;
        clear();
        return outcome;
    }
    public boolean timeout(long id) {
        if (!matches(id)) return false;
        clear();
        return true;
    }
    public void clear() { pending = null; pendingId = 0; }
}
