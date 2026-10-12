package cn.dooraccess.app;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RadialGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.view.View;

/** Decorative native rendering of the approved status ring; never changes lock state. */
public final class StatusHaloView extends View {
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF arc = new RectF();
    private boolean confirmed, unlocked;
    private float centerX, centerY, radius;
    private RadialGradient glow;

    public StatusHaloView(Context context, AttributeSet attributes) {
        super(context, attributes);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    public void setStatus(boolean nextConfirmed, boolean nextUnlocked) {
        if (confirmed == nextConfirmed && unlocked == nextUnlocked) return;
        confirmed = nextConfirmed;
        unlocked = nextUnlocked;
        updateGlow();
        invalidate();
    }

    @Override protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        centerX = width / 2f; centerY = height / 2f;
        radius = Math.min(width, height) * 0.43f;
        updateGlow();
    }

    private void updateGlow() {
        if (radius <= 0) return;
        glow = new RadialGradient(centerX, centerY, radius,
            Color.parseColor(unlocked ? "#163E38" : "#15364A"),
            Color.parseColor(unlocked ? "#102B29" : "#102331"), Shader.TileMode.CLAMP);
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (radius <= 0) return;
        float density = getResources().getDisplayMetrics().density;
        int accent = Color.parseColor(unlocked ? "#63E5B7" : "#5AE1F5");
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.WHITE);
        paint.setShader(glow);
        canvas.drawCircle(centerX, centerY, radius, paint);
        paint.setShader(null);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(density);
        paint.setColor(confirmed ? withAlpha(accent, 150) : Color.parseColor("#34586B"));
        canvas.drawCircle(centerX, centerY, radius, paint);
        float inner = radius - 7 * density;
        arc.set(centerX-inner, centerY-inner, centerX+inner, centerY+inner);
        paint.setColor(withAlpha(accent, confirmed ? 90 : 50));
        for (int degrees = 0; degrees < 360; degrees += 10)
            canvas.drawArc(arc, degrees, 3, false, paint);
        float outer = radius + 4 * density;
        arc.set(centerX-outer, centerY-outer, centerX+outer, centerY+outer);
        paint.setColor(withAlpha(accent, confirmed ? 165 : 95));
        paint.setStrokeWidth(1.8f * density);
        paint.setStrokeCap(Paint.Cap.ROUND);
        canvas.drawArc(arc, 195, 94, false, paint);
        canvas.drawArc(arc, 15, 94, false, paint);
        paint.setStrokeCap(Paint.Cap.BUTT);
    }

    private static int withAlpha(int color, int alpha) { return (color & 0x00ffffff) | (alpha << 24); }
}
