package art.acquarelli.engine;

import android.content.Context;
import android.media.MediaPlayer;
import android.util.AttributeSet;
import android.util.Log;
import android.widget.VideoView;

import java.lang.reflect.Field;

public class ScalableVideoView extends VideoView {

    private int mVideoWidth;
    private int mVideoHeight;
    private DisplayMode displayMode = DisplayMode.ORIGINAL;
    private MediaPlayer mp = null;

    private int mViewportWidth;
    private int mViewportHeight;

    public enum DisplayMode {
        ORIGINAL,       // original aspect ratio
        FULL_SCREEN,    // fit to screen
        ZOOM            // zoom in
    };

    public ScalableVideoView(Context context)
    {
        super(context);
    }

    public ScalableVideoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public ScalableVideoView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mVideoWidth = 0;
        mVideoHeight = 0;
    }

    public void SetViewportSize(int x, int y)
    {
        mViewportWidth = x;
        mViewportHeight = y;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = getDefaultSize(0, widthMeasureSpec);
        int height = getDefaultSize(mVideoHeight, heightMeasureSpec);
/*
        if (displayMode == DisplayMode.ORIGINAL) {
            if (mVideoWidth > 0 && mVideoHeight > 0) {
                if ( mVideoWidth * height  > width * mVideoHeight ) {
                    // video height exceeds screen, shrink it
                    height = width * mVideoHeight / mVideoWidth;
                } else if ( mVideoWidth * height  < width * mVideoHeight ) {
                    // video width exceeds screen, shrink it
                    width = height * mVideoWidth / mVideoHeight;
                } else {
                    // aspect ratio is correct
                }
            }
        }
        else if (displayMode == DisplayMode.FULL_SCREEN) {
            // just use the default screen width and screen height
        }
        else if (displayMode == DisplayMode.ZOOM) {
            // zoom video
            if (mVideoWidth > 0 && mVideoHeight > 0 && mVideoWidth < width) {
                height = mVideoHeight * width / mVideoWidth;
            }
        }

 */
        Log.i("check", String.format("calling onMeasure with size %d, %d ", width, height));
        setMeasuredDimension(width, height);
    }

    public void changeVideoSize(int width, int height)
    {
        mVideoWidth = width;
        mVideoHeight = height;

        getHolder().setFixedSize(width, height);

        requestLayout();
        invalidate();
    }

    public void setDisplayMode(DisplayMode mode) {
        displayMode = mode;
    }
}
