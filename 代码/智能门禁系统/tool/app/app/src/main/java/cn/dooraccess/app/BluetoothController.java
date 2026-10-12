package cn.dooraccess.app;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.os.Handler;
import android.os.Looper;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

/** Permission checks belong to the Activity. Socket I/O never runs on the UI thread. */
@SuppressLint("MissingPermission")
public final class BluetoothController {
    public enum State { DISCONNECTED, CONNECTING, CONNECTED }
    public interface Listener {
        void onState(State state, String message);
        void onData(byte[] data);
        void onSent(byte[] data, long requestId);
    }
    private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805f9b34fb");
    private final Object guard = new Object();
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService connector = Executors.newSingleThreadExecutor();
    private final ExecutorService reader = Executors.newSingleThreadExecutor();
    private final ExecutorService writer = Executors.newSingleThreadExecutor();
    private final ScheduledExecutorService timer = Executors.newSingleThreadScheduledExecutor();
    private final Listener listener;
    private volatile State state = State.DISCONNECTED;
    private int generation;
    private boolean closed;
    private BluetoothSocket connectingSocket;
    private BluetoothSocket socket;
    public BluetoothController(Listener listener) { this.listener = listener; }
    public State state() { return state; }

    public void connect(BluetoothDevice device) {
        disconnect("切换连接");
        final int token;
        synchronized (guard) {
            if (closed) return;
            token = ++generation;
            state = State.CONNECTING;
        }
        dispatch(token, () -> listener.onState(State.CONNECTING, "正在连接，请稍候…"));
        timer.schedule(() -> {
            synchronized (guard) {
                if (token != generation || state != State.CONNECTING || closed) return;
                fail(token, "连接超时，请确认模块已通电、已配对且没有被其他设备占用");
            }
        }, 12, TimeUnit.SECONDS);
        connector.execute(() -> {
            BluetoothSocket candidate = null;
            try {
                candidate = device.createRfcommSocketToServiceRecord(SPP_UUID);
                synchronized (guard) {
                    if (token != generation || closed) { closeSocket(candidate); return; }
                    connectingSocket = candidate;
                }
                candidate.connect();
                synchronized (guard) {
                    if (token != generation || closed) { closeSocket(candidate); return; }
                    socket = candidate;
                    connectingSocket = null;
                    state = State.CONNECTED;
                }
                dispatch(token, () -> listener.onState(State.CONNECTED, "连接已建立，点击查询获取门锁状态"));
                final BluetoothSocket connected = candidate;
                synchronized (guard) {
                    // Activity destruction may shut down the executor just as connect() returns.
                    if (closed || token != generation) return;
                    reader.execute(() -> readLoop(connected, token));
                }
            } catch (IOException | SecurityException error) {
                closeSocket(candidate);
                fail(token, "连接失败，请确认蓝牙权限、系统配对和模块 SPP 服务");
            }
        });
    }

    private void readLoop(BluetoothSocket connected, int token) {
        try {
            InputStream stream = connected.getInputStream();
            byte[] buffer = new byte[512];
            while (true) {
                synchronized (guard) { if (closed || token != generation) return; }
                int count = stream.read(buffer);
                if (count < 0) throw new IOException("Connection closed");
                if (count == 0) continue;
                byte[] data = Arrays.copyOf(buffer, count);
                dispatch(token, () -> listener.onData(data));
            }
        } catch (IOException | SecurityException error) {
            fail(token, "蓝牙连接已中断，请重新连接");
        }
    }

    public boolean send(byte[] bytes, long requestId) {
        final BluetoothSocket active;
        final int token;
        synchronized (guard) {
            if (closed || state != State.CONNECTED || socket == null) return false;
            active = socket;
            token = generation;
        }
        final byte[] payload = bytes.clone();
        writer.execute(() -> {
            synchronized (guard) { if (closed || token != generation || socket != active) return; }
            try {
                active.getOutputStream().write(payload);
                active.getOutputStream().flush();
                dispatch(token, () -> listener.onSent(payload, requestId));
            } catch (IOException | SecurityException error) {
                fail(token, "数据发送失败，连接已断开，请重新连接");
            }
        });
        return true;
    }

    private void dispatch(int token, Runnable action) {
        main.post(() -> {
            synchronized (guard) { if (closed || token != generation) return; }
            action.run();
        });
    }

    private void fail(int token, String message) {
        synchronized (guard) {
            if (closed || token != generation) return;
            // Increment before closing sockets: stale read/connect/write callbacks are discarded.
            stopLocked();
            token = generation;
        }
        final int notification = token;
        dispatch(notification, () -> listener.onState(State.DISCONNECTED, message));
    }
    public void disconnect(String reason) {
        final int token;
        synchronized (guard) {
            if (closed) return;
            stopLocked();
            token = generation;
        }
        dispatch(token, () -> listener.onState(State.DISCONNECTED, reason));
    }
    private void stopLocked() {
        generation++;
        state = State.DISCONNECTED;
        closeSocket(connectingSocket);
        closeSocket(socket);
        connectingSocket = null;
        socket = null;
    }
    private static void closeSocket(BluetoothSocket target) {
        if (target != null) try { target.close(); } catch (IOException ignored) { }
    }
    public void close() {
        synchronized (guard) {
            if (closed) return;
            stopLocked();
            closed = true;
        }
        main.removeCallbacksAndMessages(null);
        connector.shutdownNow();
        reader.shutdownNow();
        writer.shutdownNow();
        timer.shutdownNow();
    }
}
