package cn.dooraccess.app;

import android.content.Context;
import android.content.SharedPreferences;

public final class SettingsStore {
    private final SharedPreferences preferences;
    public SettingsStore(Context context) {
        preferences = context.getSharedPreferences("door_access", Context.MODE_PRIVATE);
    }
    public DoorProtocol.Config load() {
        DoorProtocol.Config defaults = DoorProtocol.Config.defaults();
        try {
            return new DoorProtocol.Config(preferences.getString("unlock", defaults.unlock),
                preferences.getString("lock", defaults.lock), preferences.getString("query", defaults.query),
                preferences.getInt("ending", 0), preferences.getString("replyUnlocked", defaults.unlockedReply),
                preferences.getString("replyLocked", defaults.lockedReply));
        } catch (IllegalArgumentException ignored) { return defaults; }
    }
    public void save(DoorProtocol.Config config) {
        preferences.edit().putString("unlock", config.unlock).putString("lock", config.lock)
            .putString("query", config.query).putInt("ending", config.ending)
            .putString("replyUnlocked", config.unlockedReply).putString("replyLocked", config.lockedReply).apply();
    }
    public void rememberDevice(String address) { preferences.edit().putString("device", address).apply(); }
    public String rememberedDevice() { return preferences.getString("device", ""); }
}
