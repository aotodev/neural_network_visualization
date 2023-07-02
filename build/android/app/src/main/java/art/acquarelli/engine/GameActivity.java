package art.acquarelli.engine;

import static com.google.android.gms.ads.RequestConfiguration.TAG_FOR_CHILD_DIRECTED_TREATMENT_TRUE;

import android.app.AlertDialog;
import android.app.NativeActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.util.AttributeSet;
import android.util.Log;

import android.os.Build;

import android.view.DisplayCutout;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.net.Uri;

import android.media.MediaPlayer;
import android.widget.LinearLayout;
import android.widget.VideoView;

import java.util.concurrent.Semaphore;
import android.util.DisplayMetrics;

import androidx.annotation.Keep;

import art.acquarelli.neuralnetworksimulation.BuildConfig;
import art.acquarelli.engine.ScalableVideoView;

// ads
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.AdView;
import com.google.android.gms.ads.MobileAds;

import com.google.android.gms.ads.AdError;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.FullScreenContentCallback;
import com.google.android.gms.ads.LoadAdError;
import androidx.annotation.NonNull;

import com.google.android.gms.ads.OnUserEarnedRewardListener;
import com.google.android.gms.ads.RequestConfiguration;
import com.google.android.gms.ads.initialization.InitializationStatus;
import com.google.android.gms.ads.initialization.OnInitializationCompleteListener;

import com.google.android.gms.ads.rewarded.RewardItem;
import com.google.android.gms.ads.rewarded.RewardedAd;
import com.google.android.gms.ads.rewarded.RewardedAdLoadCallback;

import com.google.android.gms.games.GamesSignInClient;
import com.google.android.gms.games.PlayGames;
import com.google.android.gms.games.PlayGamesSdk;
import com.google.android.gms.tasks.OnSuccessListener;

import art.acquarelli.neuralnetworksimulation.R;

public class GameActivity extends NativeActivity
{
    static
    {
        // Load native library
        System.loadLibrary("engine_cpp");
    }

    // VideoView videoView;
    ScalableVideoView videoView;

    private ScaleGestureDetector m_ScaleDetector;
    private float m_ScaleFactor = 1.0f;
    boolean finishedVideo = false;
    private Vibrator m_Vibrator;
    private LinearLayout m_LinearLayout;

    private RewardedAd m_RewardedAd;
    private boolean m_bIsAdLoading = false;

    private boolean m_IsAuthenticated = false;

    public boolean receivedAdReward = false;

    static GameActivity s_Activity;

    private int m_DisplayWidth = 0;
    private int m_DisplayHeight = 0;
    private int m_DisplayCutoutHeight = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        Log.d("INIT", "GameActivity onCreate");
        s_Activity = this;
        super.onCreate(savedInstanceState);

        SetDisplayMetrics();
        GetDisplayCutout();
        SetupAndDisplaySplashVideo();
        //InitializePlayGamesSdk();
        //InitializeMobileAds();

        m_ScaleDetector = new ScaleGestureDetector(this, new ScaleListener());
        m_Vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
    }

    @Keep
    public void DestroyApplication()
    {
        Log.d("GameActivity", "Destroying GameActivity");
        moveTaskToBack(true);
        android.os.Process.killProcess(android.os.Process.myPid());
        System.exit(1);
    }

    private void SetDisplayMetrics()
    {
        DisplayMetrics displayMetrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
        m_DisplayHeight = displayMetrics.heightPixels;
        m_DisplayWidth = displayMetrics.widthPixels;
    }

    private void GetDisplayCutout()
    {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q)
        {
            WindowInsets wInsets = s_Activity.getWindow().getDecorView().getRootWindowInsets();
            if (wInsets != null)
            {
                DisplayCutout displayCutout = wInsets.getDisplayCutout();
                if (displayCutout != null && displayCutout.getBoundingRects().size() > 0)
                {
                    Rect cutoffRect = displayCutout.getBoundingRectTop();
                    m_DisplayCutoutHeight = cutoffRect.height();
                }
            }
        }
    }

    private void SetupAndDisplaySplashVideo()
    {
        setContentView(R.layout.splash_layout);
        videoView = (ScalableVideoView) findViewById(R.id.videoView);
        videoView.SetViewportSize(m_DisplayWidth, m_DisplayHeight);

        videoView.setOnPreparedListener(new MediaPlayer.OnPreparedListener()
        {
            @Override
            public void onPrepared(MediaPlayer mediaPlayer)
            {
                videoView.changeVideoSize(m_DisplayWidth * (int)(16.0f / 9.0f), m_DisplayWidth);
                Log.v("event", "called onPrepared");
            }
        });

        String videoPath = "android.resource://" + getPackageName() + "/" + R.raw.splash;
        Uri videoURI = Uri.parse(videoPath);
        videoView.setVideoURI(videoURI);

        videoView.setOnCompletionListener(new MediaPlayer.OnCompletionListener()
        {
            @Override
            public void onCompletion(MediaPlayer mediaPlayer)
            {
                videoView.setVisibility(View.INVISIBLE);
                LinearLayout.MarginLayoutParams params = new LinearLayout.MarginLayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
                params.setMargins(0, 0, 0, 0);
                m_LinearLayout = new LinearLayout(s_Activity);
                s_Activity.setContentView(m_LinearLayout, params);

                SetVideoSplashFinished();
                finishedVideo = true;
            }
        });

        videoView.setDisplayMode((ScalableVideoView.DisplayMode.ORIGINAL));
        videoView.changeVideoSize(m_DisplayWidth * (int) (16.0f / 9.0f), m_DisplayWidth);
        videoView.start();
        RestoreTransparentBars();
    }

    private void InitializeMobileAds()
    {
        try
        {
            RequestConfiguration conf =
                new RequestConfiguration.Builder()
                    .setTagForChildDirectedTreatment(TAG_FOR_CHILD_DIRECTED_TREATMENT_TRUE)
                        .build();

            MobileAds.setRequestConfiguration(conf);
            MobileAds.initialize(this, new OnInitializationCompleteListener()
            {
                @Override
                public void onInitializationComplete(InitializationStatus initializationStatus)
                {
                }
            });
        }
        catch (Exception e)
        {
            Log.d("GameActivity", "GoogleApiClient exception caught: " + e.toString());
        }

        LoadRewardedVideoAd();
    }

    private void InitializePlayGamesSdk()
    {
        PlayGamesSdk.initialize(this);

        GamesSignInClient gamesSignInClient = PlayGames.getGamesSignInClient(s_Activity);

        gamesSignInClient.isAuthenticated().addOnCompleteListener(isAuthenticatedTask ->
        {
            m_IsAuthenticated = (isAuthenticatedTask.isSuccessful() &&
                isAuthenticatedTask.getResult().isAuthenticated());

            if (m_IsAuthenticated)
            {
                // Continue with Play Games Services
            }
            else
            {
                // Disable your integration with Play Games Services or show a
                // login button to ask  players to sign-in. Clicking it should
                // call GamesSignInClient.signIn().
            }
        });
    }

    void SignInClient()
    {
        GamesSignInClient gamesSignInClient = PlayGames.getGamesSignInClient(s_Activity);
        gamesSignInClient.signIn();

        gamesSignInClient.isAuthenticated().addOnCompleteListener(isAuthenticatedTask ->
        {
            m_IsAuthenticated = (isAuthenticatedTask.isSuccessful() &&
                isAuthenticatedTask.getResult().isAuthenticated());
        });
    }
    @Keep
    public void ShowLeaderboard()
    {
        if(!m_IsAuthenticated)
        {
            //ShowMessage("Authentication failed\nCan't display leaderboard");
            SignInClient();
            return;
        }

        PlayGames.getLeaderboardsClient(this)
            .getLeaderboardIntent(getString(R.string.leaderboard_id))
            .addOnSuccessListener(new OnSuccessListener<Intent>()
            {
                @Override
                public void onSuccess(Intent intent)
                {
                    startActivityForResult(intent, 1);
                }
            });
    }
    @Keep
    public void UpdateLeaderboardScore(int score)
    {
        if(!m_IsAuthenticated)
        {
            // TODO: not authenticated, prompt the user to login
            return;
        }

        PlayGames.getLeaderboardsClient(this).submitScore(getString(R.string.leaderboard_id), score);
    }

    @Keep
    public int GetDisplayWidth()
    {
        if(m_DisplayWidth == 0)
        {
            SetDisplayMetrics();
        }

        return m_DisplayWidth;
    }

    @Keep
    public int GetDisplayHeight()
    {
        if(m_DisplayHeight == 0)
        {
            SetDisplayMetrics();
        }

        return m_DisplayHeight;
    }

    @Keep
    public int GetDisplayCutoutHeight()
    {
        if(m_DisplayCutoutHeight == 0)
        {
            GetDisplayCutout();
        }

        return m_DisplayCutoutHeight;
    }

    @Override
    public void onBackPressed()
    {
        AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(this);
        alertDialogBuilder.setTitle("Exit Application?");

        alertDialogBuilder.setMessage("Click yes to exit!").setCancelable(false)
                .setPositiveButton("Yes",
                        new DialogInterface.OnClickListener()
                        {
                            public void onClick(DialogInterface dialog, int id)
                            {
                                NativeRequestDestroy();
                            }
                        })

                .setNegativeButton("No", new DialogInterface.OnClickListener()
                {
                    public void onClick(DialogInterface dialog, int id)
                    {
                        dialog.cancel();
                    }
                });

        AlertDialog alertDialog = alertDialogBuilder.create();
        alertDialog.show();
    }

    // Use a semaphore to create a modal dialog
    private final Semaphore semaphore = new Semaphore(0, true);

    @Keep
    public void ShowMessage(final String message)
    {
        final GameActivity activity = this;

        ApplicationInfo applicationInfo = activity.getApplicationInfo();
        final String applicationName = applicationInfo.nonLocalizedLabel.toString();

        this.runOnUiThread(new Runnable()
        {
           public void run()
           {
               AlertDialog.Builder builder = new AlertDialog.Builder(activity, android.R.style.Theme_Material_Dialog_Alert);
               builder.setTitle(applicationName);
               builder.setMessage(message);
               builder.setPositiveButton("Close", new DialogInterface.OnClickListener()
               {
                   public void onClick(DialogInterface dialog, int id)
                   {
                       semaphore.release();
                   }
               });

               builder.setCancelable(false);
               AlertDialog dialog = builder.create();
               dialog.show();
           }
        });

        try
        {
            semaphore.acquire();
        }
        catch (InterruptedException e) { }

        RestoreTransparentBars();
    }

    @Keep
    public void Vibrate()
    {
        m_Vibrator.vibrate(VibrationEffect.createOneShot(200, VibrationEffect.DEFAULT_AMPLITUDE));
    }

    @Keep
    public void RestoreTransparentBars()
    {
        try
        {
            View decorView = getWindow().getDecorView();

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
            {
                getWindow().setDecorFitsSystemWindows(false);
                WindowInsetsController controller = getWindow().getInsetsController();
                if (controller != null)
                {
                    controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                    controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                }
            }
            else
            {
                decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
            }
        }
        catch (Exception e) {}
    }

    @Override
    protected void onPause()
    {
        super.onPause();
        Log.d("resume", "GameActivity onPause");
    }
    @Override
    protected void onResume()
    {
        super.onResume();
        Log.d("resume", "GameActivity onResume");
        RestoreTransparentBars();
        if(!finishedVideo)
        {
            videoView.seekTo(0);
            videoView.start();
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus)
    {
        Log.d("GameActivity", "GameActivity onWindowFocusChanged");
        super.onWindowFocusChanged(hasFocus);
        if(hasFocus)
        {
            RestoreTransparentBars();
        }
        else
        {
            //super.onPause();
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev)
    {
        super.onTouchEvent(ev);
        // Let the ScaleGestureDetector inspect all events.
        return m_ScaleDetector.onTouchEvent(ev);
    }

    // Native functions
    public native void SetVideoSplashFinished();
    public native void NativeRequestDestroy();
    public native void SetAdState(int state);
    public native void CallOnVideoAdReward(boolean receivedReward);
    public native void NativeOnPinchScale(float scale);

    public void LoadRewardedVideoAd()
    {
        s_Activity.runOnUiThread(new Runnable()
        {
            @Override
            public void run()
            {
                try
                {
                    s_Activity.LoadRewardedVideoAd_Internal();
                }
                catch (Exception e)
                {
                    Log.d("ad exception", "RewardedVideoAd exception caught: " + e);
                }
            }
        });
    }

    public void ShowRewardedVideoAd()
    {
        s_Activity.runOnUiThread(new Runnable()
        {
            @Override
            public void run()
            {
                try
                {
                    s_Activity.ShowRewardedVideoAd_Internal();
                }
                catch (Exception e)
                {
                    Log.d("ad exception", "RewardedVideoAd exception caught: " + e);
                }
            }
        });
    }

    private void LoadRewardedVideoAd_Internal()
    {
        if (m_RewardedAd == null)
        {
            m_bIsAdLoading = true;
            SetAdState(1);
            AdRequest adRequest = new AdRequest.Builder().build();

            RewardedAd.load(this, BuildConfig.ADMOB_UNIT_ID, adRequest, new RewardedAdLoadCallback()
            {
                @Override
                public void onAdFailedToLoad(@NonNull LoadAdError loadAdError)
                {
                    // Handle the error.
                    Log.d("REWARD AD", loadAdError.getMessage());
                    m_bIsAdLoading = false;
                    SetAdState(3);
                    GameActivity.this.m_RewardedAd = null;
                }

                @Override
                public void onAdLoaded(@NonNull RewardedAd rewardedAd)
                {
                    m_RewardedAd = rewardedAd;
                    m_bIsAdLoading = false;
                    SetAdState(2);
                    Log.d("REWARD AD", "onAdLoaded");
                }
            });
        }
    }

    private void ShowRewardedVideoAd_Internal()
    {
        if (m_RewardedAd == null)
        {
            Log.d("SHOW AD", "The rewarded ad wasn't ready yet.");
            return;
        }

        m_RewardedAd.setFullScreenContentCallback(new FullScreenContentCallback()
        {
            @Override
            public void onAdShowedFullScreenContent()
            {
                // Called when ad is shown.
                Log.d("SHOW AD", "onAdShowedFullScreenContent");
            }

            @Override
            public void onAdFailedToShowFullScreenContent(AdError adError)
            {
                Log.d("SHOW AD", "onAdFailedToShowFullScreenContent");
                m_RewardedAd = null;
                GameActivity.this.LoadRewardedVideoAd();
            }

            @Override
            public void onAdDismissedFullScreenContent()
            {
                m_RewardedAd = null;
                Log.d("SHOW AD", "onAdDismissedFullScreenContent");
                GameActivity.this.CallOnVideoAdReward(GameActivity.this.receivedAdReward);
                GameActivity.this.LoadRewardedVideoAd();

                GameActivity.this.receivedAdReward = false;
            }
        });

        m_RewardedAd.show(GameActivity.this, new OnUserEarnedRewardListener()
        {
            @Override
            public void onUserEarnedReward(@NonNull RewardItem rewardItem)
            {
                // Handle the reward.
                Log.d("TAG", "The user earned the reward.");
                int rewardAmount = rewardItem.getAmount();
                String rewardType = rewardItem.getType();

                GameActivity.this.receivedAdReward = true;
                GameActivity.this.LoadRewardedVideoAd();
            }
        });
    }

    private class ScaleListener
            extends ScaleGestureDetector.SimpleOnScaleGestureListener
    {
        @Override
        public boolean onScale(ScaleGestureDetector detector)
        {
            m_ScaleFactor *= detector.getScaleFactor();
            float span = detector.getCurrentSpan() - detector.getPreviousSpan();

            // Don't let the object get too small or too large.
            m_ScaleFactor = Math.max(0.01f, Math.min(m_ScaleFactor, 100.0f));
            NativeOnPinchScale(span);
            return true;
        }
    }
}

