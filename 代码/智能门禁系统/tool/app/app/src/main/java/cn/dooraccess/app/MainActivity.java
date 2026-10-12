package cn.dooraccess.app;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.RelativeSizeSpan;
import android.view.View;
import android.view.WindowInsets;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public final class MainActivity extends Activity implements BluetoothController.Listener {
    private static final int REQUEST_PERMISSION = 10;
    private static final int REQUEST_ENABLE = 11;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final CommandTracker tracker = new CommandTracker();
    private final ArrayDeque<String> logs = new ArrayDeque<>();
    private final View[] pages = new View[3];
    private final TextView[] tabs = new TextView[3];
    private BluetoothAdapter adapter;
    private BluetoothController bluetooth;
    private BluetoothController.State observedState = BluetoothController.State.DISCONNECTED;
    private SettingsStore store;
    private DoorProtocol.Config config;
    private DoorProtocol.FrameDecoder decoder;
    private FrameLayout container;
    private TextView title, badge, lockState, lockDetail, stateTag, feedbackDot, actionHint, currentDevice, currentDeviceDetail, controlConnection, deviceHint, settingsHint, logText;
    private ImageView lockIcon;
    private StatusHaloView statusHalo;
    private Button manageDevice;
    private Button unlockButton, lockButton, queryButton, exitButton, manualSend, disconnectButton;
    private LinearLayout deviceList;
    private EditText commandUnlock, commandLock, commandQuery, replyUnlocked, replyLocked, manualInput;
    private Spinner ending, sendMode;
    private int currentPage;
    private DoorProtocol.Reply reportedState = DoorProtocol.Reply.OTHER;
    private String stateTime = "";
    private String selectedName = "";
    private String selectedAddress = "";
    private String feedback = "请先连接蓝牙设备";
    private enum FeedbackTone { NORMAL, SUCCESS, WARNING, ERROR }
    private FeedbackTone feedbackTone = FeedbackTone.NORMAL;
    private Runnable requestTimeout;
    private boolean dataSending, receiverRegistered, destroyed, logUpdateScheduled;
    private String dataLabel = "手动数据";

    private final BroadcastReceiver receiver = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            if (BluetoothAdapter.ACTION_STATE_CHANGED.equals(intent.getAction())) {
                if (intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR) != BluetoothAdapter.STATE_ON) {
                    cancelWaiting();
                    bluetooth.disconnect("手机蓝牙已关闭，连接已断开");
                }
                refreshDevices(false);
            }
        }
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        setContentView(R.layout.activity_main);
        configureInsets();
        BluetoothManager manager = (BluetoothManager) getSystemService(BLUETOOTH_SERVICE);
        adapter = manager == null ? null : manager.getAdapter();
        store = new SettingsStore(this);
        config = store.load();
        bluetooth = new BluetoothController(this);
        decoder = new DoorProtocol.FrameDecoder(new DoorProtocol.FrameListener() {
            @Override public void onLine(String line) { receiveLine(line); }
            @Override public void onOversizedFrame() { addLog("提示", "收到超长数据，已丢弃至下一个换行符"); }
        });
        container = findViewById(R.id.page_container);
        title = findViewById(R.id.page_title);
        badge = findViewById(R.id.connection_badge);
        pages[0] = getLayoutInflater().inflate(R.layout.page_control, container, false);
        pages[1] = getLayoutInflater().inflate(R.layout.page_devices, container, false);
        pages[2] = getLayoutInflater().inflate(R.layout.page_settings, container, false);
        tabs[0] = findViewById(R.id.nav_control);
        tabs[1] = findViewById(R.id.nav_device);
        tabs[2] = findViewById(R.id.nav_settings);
        for (int i = 0; i < tabs.length; i++) {
            final int page = i;
            tabs[i].setOnClickListener(view -> showPage(page));
        }
        bindControl();
        bindDevices();
        bindSettings();
        if (Build.VERSION.SDK_INT >= 26) {
            for (EditText field : new EditText[] {commandUnlock, commandLock, commandQuery, replyUnlocked, replyLocked, manualInput})
                field.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);
        }
        showPage(state == null ? 0 : Math.max(0, Math.min(2, state.getInt("page", 0))));
        render();
    }

    private void configureInsets() {
        getWindow().getDecorView().setSystemUiVisibility(0);
        getWindow().setStatusBarColor(getColor(R.color.page));
        getWindow().setNavigationBarColor(getColor(R.color.nav));
        if (Build.VERSION.SDK_INT >= 29) getWindow().setNavigationBarContrastEnforced(false);
        if (Build.VERSION.SDK_INT >= 30) {
            getWindow().setDecorFitsSystemWindows(false);
            findViewById(R.id.root).setOnApplyWindowInsetsListener((view, insets) -> {
                android.graphics.Insets safe = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.ime());
                view.setPadding(safe.left, safe.top, safe.right, safe.bottom);
                return insets;
            });
        }
    }

    private void bindControl() {
        View page = pages[0];
        lockState = page.findViewById(R.id.lock_state);
        lockDetail = page.findViewById(R.id.lock_detail);
        lockIcon = page.findViewById(R.id.lock_icon);
        statusHalo = page.findViewById(R.id.status_halo);
        stateTag = page.findViewById(R.id.state_tag);
        feedbackDot = page.findViewById(R.id.feedback_dot);
        actionHint = page.findViewById(R.id.action_hint);
        currentDevice = page.findViewById(R.id.current_device);
        currentDeviceDetail = page.findViewById(R.id.current_device_detail);
        controlConnection = page.findViewById(R.id.control_connection);
        manageDevice = page.findViewById(R.id.manage_device);
        manageDevice.setOnClickListener(view -> showPage(1));
        unlockButton = page.findViewById(R.id.unlock_button);
        lockButton = page.findViewById(R.id.lock_button);
        queryButton = page.findViewById(R.id.query_button);
        exitButton = page.findViewById(R.id.exit_button);
        setButtonSubtitle(unlockButton, R.string.page_control_text_05, R.string.unlock_subtitle);
        setButtonSubtitle(lockButton, R.string.page_control_text_06, R.string.lock_subtitle);
        unlockButton.setOnClickListener(view -> sendOperation(DoorProtocol.Operation.UNLOCK));
        lockButton.setOnClickListener(view -> sendOperation(DoorProtocol.Operation.LOCK));
        queryButton.setOnClickListener(view -> sendOperation(DoorProtocol.Operation.QUERY));
        exitButton.setOnClickListener(view -> {
            if (canSend()) sendData(config.exitPayload(), "退出指令");
        });
        page.findViewById(R.id.go_devices).setOnClickListener(view -> showPage(1));
    }

    private void bindDevices() {
        View page = pages[1];
        deviceList = page.findViewById(R.id.device_list);
        deviceHint = page.findViewById(R.id.device_hint);
        disconnectButton = page.findViewById(R.id.disconnect_button);
        page.findViewById(R.id.system_bluetooth).setOnClickListener(view -> {
            try { startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS)); }
            catch (android.content.ActivityNotFoundException error) { toast("无法打开蓝牙设置，请从手机设置中进入"); }
        });
        page.findViewById(R.id.refresh_devices).setOnClickListener(view -> refreshDevices(true));
        disconnectButton.setOnClickListener(view -> {
            cancelWaiting();
            bluetooth.disconnect("连接已手动断开");
        });
    }

    private void bindSettings() {
        View page = pages[2];
        commandUnlock = page.findViewById(R.id.command_unlock);
        commandLock = page.findViewById(R.id.command_lock);
        commandQuery = page.findViewById(R.id.command_query);
        replyUnlocked = page.findViewById(R.id.reply_unlocked);
        replyLocked = page.findViewById(R.id.reply_locked);
        ending = page.findViewById(R.id.line_ending);
        sendMode = page.findViewById(R.id.send_mode);
        manualInput = page.findViewById(R.id.manual_input);
        manualSend = page.findViewById(R.id.manual_send);
        settingsHint = page.findViewById(R.id.settings_hint);
        logText = page.findViewById(R.id.log_text);
        ending.setAdapter(spinnerAdapter(new String[] {"CRLF  (\\r\\n，默认)", "LF  (\\n)", "CR  (\\r)", "无结束符"}));
        sendMode.setAdapter(spinnerAdapter(new String[] {"文本（UTF-8）", "HEX 原始字节"}));
        loadFields();
        page.findViewById(R.id.save_settings).setOnClickListener(view -> saveFields());
        page.findViewById(R.id.reset_settings).setOnClickListener(view -> {
            if (isBusy()) { toast("请等待当前发送完成"); return; }
            new AlertDialog.Builder(this).setTitle("恢复默认指令？")
                .setMessage("开锁 UNLOCK、上锁 LOCK、查询 STATUS，结束符 CRLF。")
                .setNegativeButton("取消", null).setPositiveButton("恢复", (dialog, which) -> {
                    config = DoorProtocol.Config.defaults();
                    store.save(config);
                    decoder.reset();
                    reportedState = DoorProtocol.Reply.OTHER;
                    loadFields();
                    settingsHint.setText("已恢复默认配置");
                    addLog("配置", "已恢复默认指令和状态回执");
                    render();
                }).show();
        });
        manualSend.setOnClickListener(view -> sendManual());
        page.findViewById(R.id.clear_log).setOnClickListener(view -> { logs.clear(); updateLog(); });
    }

    private void loadFields() {
        commandUnlock.setText(config.unlock); commandLock.setText(config.lock); commandQuery.setText(config.query);
        replyUnlocked.setText(config.unlockedReply); replyLocked.setText(config.lockedReply); ending.setSelection(config.ending);
    }
    private ArrayAdapter<String> spinnerAdapter(String[] choices) {
        ArrayAdapter<String> result = new ArrayAdapter<>(this, R.layout.spinner_item, choices);
        result.setDropDownViewResource(R.layout.spinner_dropdown);
        return result;
    }
    private void setButtonSubtitle(Button button, int labelResource, int subtitleResource) {
        String label = getString(labelResource);
        SpannableString text = new SpannableString(label + "\n" + getString(subtitleResource));
        text.setSpan(new RelativeSizeSpan(0.65f), label.length() + 1, text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        button.setText(text);
    }
    private void setFeedback(String text, FeedbackTone tone) {
        feedback = text;
        feedbackTone = tone;
    }
    private void saveFields() {
        if (isBusy()) { toast("请等待当前发送完成后再保存配置"); return; }
        try {
            DoorProtocol.Config next = new DoorProtocol.Config(commandUnlock.getText().toString(),
                commandLock.getText().toString(), commandQuery.getText().toString(), ending.getSelectedItemPosition(),
                replyUnlocked.getText().toString(), replyLocked.getText().toString());
            store.save(next);
            config = next;
            decoder.reset();
            reportedState = DoorProtocol.Reply.OTHER;
            loadFields();
            settingsHint.setText("配置已保存，之后的发送使用此配置");
            addLog("配置", "指令和回执配置已保存");
            hideKeyboard();
            render();
        } catch (IllegalArgumentException error) { settingsHint.setText(error.getMessage()); }
    }

    private void showPage(int page) {
        hideKeyboard();
        currentPage = page;
        container.removeAllViews();
        container.addView(pages[page]);
        String[] titles = {"门禁助手", "设备连接", "设置与联调"};
        title.setText(titles[page]);
        for (int i = 0; i < tabs.length; i++) {
            int tint = getColor(i == page ? R.color.green : R.color.muted);
            tabs[i].setTextColor(tint);
            Drawable icon = tabs[i].getCompoundDrawablesRelative()[1];
            if (icon != null) icon.mutate().setTint(tint);
            tabs[i].setSelected(i == page);
        }
        if (page == 1) refreshDevices(false);
        if (page == 2) updateLog();
    }
    private boolean hasPermission() {
        return Build.VERSION.SDK_INT < 31 || checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
    }

    @SuppressLint("MissingPermission")
    private void refreshDevices(boolean request) {
        deviceList.removeAllViews();
        if (adapter == null) { deviceHint.setText("这台手机不支持经典蓝牙"); return; }
        if (!hasPermission()) {
            deviceHint.setText("请点击刷新，允许访问附近设备后查看已配对列表");
            if (request) requestPermissions(new String[] {Manifest.permission.BLUETOOTH_CONNECT}, REQUEST_PERMISSION);
            return;
        }
        try {
            if (!adapter.isEnabled()) {
                deviceHint.setText("手机蓝牙尚未开启，请开启后刷新");
                if (request) startActivityForResult(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE), REQUEST_ENABLE);
                return;
            }
            List<BluetoothDevice> devices = new ArrayList<>(adapter.getBondedDevices());
            String remembered = store.rememberedDevice();
            Collections.sort(devices, (left, right) -> {
                int leftPriority = left.getAddress().equals(remembered) ? 0 : deviceName(left).toUpperCase(Locale.ROOT).contains("JDY") ? 1 : 2;
                int rightPriority = right.getAddress().equals(remembered) ? 0 : deviceName(right).toUpperCase(Locale.ROOT).contains("JDY") ? 1 : 2;
                int comparison = Integer.compare(leftPriority, rightPriority);
                return comparison != 0 ? comparison : deviceName(left).compareTo(deviceName(right));
            });
            deviceHint.setText(devices.isEmpty() ? "没有已配对设备，请先到手机蓝牙设置中配对 JDY-31"
                : "找到 " + devices.size() + " 个已配对设备，点击连接对应的门锁");
            for (BluetoothDevice device : devices) addDeviceRow(device);
        } catch (SecurityException error) { deviceHint.setText("蓝牙权限不可用，请在手机应用设置中允许附近设备权限"); }
    }

    @SuppressLint("MissingPermission")
    private static String deviceName(BluetoothDevice device) {
        String name = device.getName();
        return name == null || name.isEmpty() ? "未命名的设备" : name;
    }

    @SuppressLint("MissingPermission")
    private void addDeviceRow(BluetoothDevice device) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(18), dp(18), dp(18), dp(18));
        card.setBackgroundResource(R.drawable.bg_card);
        LinearLayout.LayoutParams layout = new LinearLayout.LayoutParams(-1, -2);
        layout.bottomMargin = dp(12);
        card.setLayoutParams(layout);
        LinearLayout header = new LinearLayout(this); header.setGravity(android.view.Gravity.CENTER_VERTICAL);
        LinearLayout.LayoutParams headerLayout = new LinearLayout.LayoutParams(-1, -2); headerLayout.bottomMargin = dp(16); header.setLayoutParams(headerLayout);
        ImageView icon = new ImageView(this); icon.setImageResource(R.drawable.ic_bluetooth); icon.setColorFilter(getColor(R.color.green));
        icon.setBackgroundResource(R.drawable.bg_device_icon); icon.setPadding(dp(10), dp(10), dp(10), dp(10));
        icon.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO); header.addView(icon, new LinearLayout.LayoutParams(dp(40), dp(40)));
        LinearLayout textColumn = new LinearLayout(this); textColumn.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams textLayout = new LinearLayout.LayoutParams(0, -2, 1); textLayout.setMarginStart(dp(12)); header.addView(textColumn, textLayout);
        TextView name = new TextView(this); name.setText(deviceName(device)); name.setTextSize(15); name.setTextColor(getColor(R.color.ink));
        TextView address = new TextView(this); address.setText(device.getAddress()); address.setTextSize(12); address.setTextColor(getColor(R.color.muted)); address.setPadding(0, dp(5), 0, 0);
        address.setTypeface(android.graphics.Typeface.MONOSPACE); textColumn.addView(name); textColumn.addView(address);
        Button connect = new Button(this, null, 0, R.style.Action);
        connect.setLayoutParams(new LinearLayout.LayoutParams(-1, -2));
        boolean selected = device.getAddress().equals(selectedAddress);
        BluetoothController.State state = bluetooth.state();
        connect.setText(selected && state == BluetoothController.State.CONNECTED ? "已连接"
            : selected && state == BluetoothController.State.CONNECTING ? "正在连接…" : "连接设备");
        connect.setEnabled(state != BluetoothController.State.CONNECTING && !(selected && state == BluetoothController.State.CONNECTED));
        connect.setAlpha(connect.isEnabled() ? 1f : 0.55f);
        connect.setOnClickListener(view -> {
            if (!hasPermission()) { refreshDevices(true); return; }
            try {
                if (!adapter.isEnabled()) { refreshDevices(true); return; }
                cancelWaiting();
                reportedState = DoorProtocol.Reply.OTHER;
                decoder.reset();
                selectedName = deviceName(device); selectedAddress = device.getAddress();
                store.rememberDevice(selectedAddress);
                // Discovery isn't started by this App. On legacy Android we can also cancel system discovery.
                if (Build.VERSION.SDK_INT < 31) adapter.cancelDiscovery();
                bluetooth.connect(device);
                render();
            } catch (SecurityException error) { toast("请在应用设置中允许蓝牙权限"); }
        });
        card.addView(header); card.addView(connect); deviceList.addView(card);
    }

    private boolean isBusy() { return tracker.isPending() || dataSending; }
    private boolean canSend() {
        if (bluetooth.state() != BluetoothController.State.CONNECTED || observedState != BluetoothController.State.CONNECTED) { toast("请先连接门锁"); return false; }
        if (isBusy()) { toast("请等待上一条发送完成"); return false; }
        return true;
    }

    private void sendOperation(DoorProtocol.Operation operation) {
        if (!canSend()) return;
        long id = tracker.begin(operation);
        if (operation != DoorProtocol.Operation.QUERY) reportedState = DoorProtocol.Reply.OTHER;
        setFeedback("正在发送，等待门锁回执…", FeedbackTone.NORMAL);
        requestTimeout = () -> {
            if (tracker.timeout(id)) {
                reportedState = DoorProtocol.Reply.OTHER;
                setFeedback("5 秒内未收到有效回执，执行结果未确认；请重新连接并查询状态", FeedbackTone.WARNING);
                addLog("超时", feedback);
                // Closing prevents a late reply or stalled write from confirming a later request.
                bluetooth.disconnect(feedback);
                render();
            }
        };
        main.postDelayed(requestTimeout, 5000);
        if (!bluetooth.send(config.payload(operation), id)) {
            cancelWaiting(); setFeedback("发送前连接已断开，请重新连接", FeedbackTone.WARNING);
        }
        render();
    }

    private void sendManual() {
        if (!canSend()) return;
        try {
            byte[] data = sendMode.getSelectedItemPosition() == 1 ? DoorProtocol.hexPayload(manualInput.getText().toString())
                : DoorProtocol.textPayload(manualInput.getText().toString(), config.ending);
            sendData(data, "手动数据");
            hideKeyboard();
        } catch (IllegalArgumentException error) { toast(error.getMessage()); }
    }

    // Exit has no agreed acknowledgement; only bound the transport write, not the MCU reply.
    private void sendData(byte[] data, String label) {
        dataSending = true;
        dataLabel = label;
        reportedState = DoorProtocol.Reply.OTHER;
        setFeedback("正在发送" + label + "…", FeedbackTone.NORMAL);
        requestTimeout = () -> {
            if (dataSending) {
                dataSending = false;
                setFeedback(label + "发送超时，结果未确认，请重新连接", FeedbackTone.WARNING);
                bluetooth.disconnect(feedback);
                addLog("超时", feedback);
                render();
            }
        };
        main.postDelayed(requestTimeout, 5000);
        if (!bluetooth.send(data, -1)) { cancelWaiting(); setFeedback("连接已断开，请重新连接", FeedbackTone.WARNING); }
        render();
    }

    @Override public void onState(BluetoothController.State state, String message) {
        if (destroyed) return;
        observedState = state;
        cancelWaiting(); decoder.reset();
        reportedState = DoorProtocol.Reply.OTHER; stateTime = "";
        setFeedback(message, message.equals(feedback) ? feedbackTone : FeedbackTone.NORMAL);
        addLog("连接", message);
        render(); refreshDevices(false);
        if (state == BluetoothController.State.CONNECTED) showPage(0);
    }

    @Override public void onData(byte[] data) {
        if (destroyed) return;
        addLog("RX", printable(data) + "\nHEX " + DoorProtocol.hex(data, 64));
        decoder.feed(data);
    }

    @Override public void onSent(byte[] data, long requestId) {
        if (destroyed) return;
        addLog("TX", printable(data) + "\nHEX " + DoorProtocol.hex(data, 64));
        if (requestId == -1 && dataSending) {
            dataSending = false;
            if (requestTimeout != null) main.removeCallbacks(requestTimeout);
            setFeedback(dataLabel.equals("退出指令")
                ? "退出指令已发送，请查看门锁是否返回菜单"
                : "手动数据已发送；是否执行成功需要门锁回执确认", FeedbackTone.NORMAL);
            if (!dataLabel.equals("退出指令")) toast("数据已发送");
        } else if (tracker.matches(requestId)) setFeedback("指令已发送，等待门锁回执…", FeedbackTone.NORMAL);
        render();
    }

    private void receiveLine(String line) {
        addLog("报文", line);
        DoorProtocol.Reply reply = config.parse(line);
        if (reply == DoorProtocol.Reply.LOCKED || reply == DoorProtocol.Reply.UNLOCKED) {
            reportedState = reply; stateTime = time();
        }
        CommandTracker.Outcome outcome = tracker.receive(reply);
        if (outcome != CommandTracker.Outcome.NONE) {
            if (requestTimeout != null) main.removeCallbacks(requestTimeout);
            if (outcome == CommandTracker.Outcome.SUCCESS) setFeedback("门锁已确认：" + stateLabel(), FeedbackTone.SUCCESS);
            else if (outcome == CommandTracker.Outcome.WRONG_STATE) setFeedback("收到门锁回执，但返回状态与本次操作不一致：" + stateLabel(), FeedbackTone.WARNING);
            else { reportedState = DoorProtocol.Reply.OTHER; setFeedback("门锁拒绝执行：" + clip(line.substring(4), 120), FeedbackTone.ERROR); }
        } else if (reply == DoorProtocol.Reply.ERROR) setFeedback("门锁返回错误：" + clip(line.substring(4), 120), FeedbackTone.ERROR);
        else if (!tracker.isPending() && !dataSending && reply != DoorProtocol.Reply.OTHER) setFeedback("已收到门锁状态更新", FeedbackTone.SUCCESS);
        render();
    }

    private void cancelWaiting() {
        if (requestTimeout != null) main.removeCallbacks(requestTimeout);
        tracker.clear(); dataSending = false;
    }
    private String stateLabel() {
        return reportedState == DoorProtocol.Reply.LOCKED ? "已上锁" : reportedState == DoorProtocol.Reply.UNLOCKED ? "已解锁" : "状态未确认";
    }
    private void render() {
        BluetoothController.State state = bluetooth.state();
        if (state == BluetoothController.State.CONNECTED && observedState != BluetoothController.State.CONNECTED) state = BluetoothController.State.CONNECTING;
        boolean connected = state == BluetoothController.State.CONNECTED;
        badge.setText(connected ? "已连接" : state == BluetoothController.State.CONNECTING ? "连接中" : "未连接");
        badge.setSelected(connected);
        badge.setTextColor(getColor(connected ? R.color.mint : R.color.muted));
        boolean confirmed = reportedState == DoorProtocol.Reply.LOCKED || reportedState == DoorProtocol.Reply.UNLOCKED;
        boolean unlocked = reportedState == DoorProtocol.Reply.UNLOCKED;
        statusHalo.setStatus(confirmed, unlocked);
        int stateColor = getColor(confirmed ? unlocked ? R.color.mint : R.color.green : R.color.muted);
        stateTag.setText(confirmed ? unlocked ? R.string.state_unlocked : R.string.state_locked : R.string.state_unknown);
        stateTag.setTextColor(stateColor);
        lockState.setText(stateLabel());
        lockIcon.setImageResource(reportedState == DoorProtocol.Reply.UNLOCKED ? R.drawable.ic_unlock : R.drawable.ic_lock);
        lockIcon.setColorFilter(stateColor);
        lockDetail.setText(reportedState == DoorProtocol.Reply.OTHER
            ? connected ? "等待有效回执，请查询状态" : "连接门锁后，点击查询状态"
            : "门锁回执更新于 " + stateTime);
        actionHint.setText(feedback);
        int feedbackColor = getColor(feedbackTone == FeedbackTone.SUCCESS ? R.color.mint : feedbackTone == FeedbackTone.WARNING ? R.color.warning : feedbackTone == FeedbackTone.ERROR ? R.color.error : R.color.muted);
        actionHint.setTextColor(feedbackColor); feedbackDot.setTextColor(feedbackColor);
        currentDevice.setText(selectedName.isEmpty() ? "尚未连接设备" : selectedName);
        currentDeviceDetail.setText(selectedAddress.isEmpty() ? getString(R.string.select_paired_device) : selectedAddress + (connected ? " · 已连接" : " · 未连接"));
        controlConnection.setText(connected ? selectedName + " · 蓝牙已连接" : state == BluetoothController.State.CONNECTING ? "正在连接蓝牙设备…" : getString(R.string.bluetooth_disconnected));
        manageDevice.setText(connected ? R.string.manage_short : R.string.connect_short);
        boolean enabled = connected && !isBusy();
        for (Button button : new Button[] {unlockButton, lockButton, queryButton, exitButton, manualSend}) {
            button.setEnabled(enabled); button.setAlpha(enabled ? 1f : 0.45f);
        }
        disconnectButton.setEnabled(state != BluetoothController.State.DISCONNECTED);
        disconnectButton.setAlpha(disconnectButton.isEnabled() ? 1f : 0.45f);
    }

    private void addLog(String direction, String text) {
        logs.addLast(time() + "  " + direction + "\n" + clip(text, 600));
        while (logs.size() > 120) logs.removeFirst();
        if (currentPage == 2 && !logUpdateScheduled) {
            logUpdateScheduled = true;
            main.postDelayed(() -> { logUpdateScheduled = false; if (!destroyed) updateLog(); }, 200);
        }
    }
    private void updateLog() {
        StringBuilder text = new StringBuilder();
        java.util.Iterator<String> iterator = logs.descendingIterator();
        while (iterator.hasNext()) {
            if (text.length() > 0) text.append("\n\n");
            text.append(iterator.next());
        }
        logText.setText(text.length() == 0 ? "等待连接设备…" : text.toString());
    }
    private static String printable(byte[] bytes) {
        String text = new String(bytes, StandardCharsets.UTF_8).replace("\r", "\\r").replace("\n", "\\n").replace("\t", "\\t");
        return clip(text.replaceAll("[\\p{Cntrl}]", "·"), 200);
    }
    private static String clip(String text, int max) { return text.length() <= max ? text : text.substring(0, max) + "…"; }
    private static String time() { return new SimpleDateFormat("HH:mm:ss", Locale.CHINA).format(new Date()); }
    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
    private void toast(String message) { Toast.makeText(this, message, Toast.LENGTH_SHORT).show(); }
    private void hideKeyboard() {
        View focused = getCurrentFocus();
        if (focused != null) {
            InputMethodManager input = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
            if (input != null) input.hideSoftInputFromWindow(focused.getWindowToken(), 0);
            focused.clearFocus();
        }
    }

    @Override protected void onStart() {
        super.onStart();
        IntentFilter filter = new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(receiver, filter, Context.RECEIVER_EXPORTED);
        else registerReceiver(receiver, filter);
        receiverRegistered = true;
        refreshDevices(false);
    }
    @Override protected void onStop() {
        if (receiverRegistered) { unregisterReceiver(receiver); receiverRegistered = false; }
        cancelWaiting();
        if (bluetooth.state() != BluetoothController.State.DISCONNECTED) bluetooth.disconnect("App 已进入后台，蓝牙连接已断开");
        super.onStop();
    }
    @Override protected void onDestroy() {
        destroyed = true; cancelWaiting();
        main.removeCallbacksAndMessages(null);
        bluetooth.close();
        super.onDestroy();
    }
    @Override protected void onSaveInstanceState(Bundle state) { state.putInt("page", currentPage); super.onSaveInstanceState(state); }
    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == REQUEST_PERMISSION) {
            if (hasPermission()) refreshDevices(true);
            else {
                deviceHint.setText("未获得附近设备权限。可在手机应用设置中允许，然后返回刷新。");
                new AlertDialog.Builder(this).setTitle("需要附近设备权限")
                    .setMessage("该权限用于读取已配对设备并与门锁通信。")
                    .setNegativeButton("稍后", null).setPositiveButton("应用设置", (dialog, which) -> {
                        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                            android.net.Uri.parse("package:" + getPackageName()));
                        startActivity(intent);
                    }).show();
            }
        }
    }
    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_ENABLE) refreshDevices(false);
    }
}
