package group.matoya.merton;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.content.Context;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.util.Log;

class AppThread extends Thread {
	native void mty_start(String name);

	public void run() {
		mty_start("merton");
	}
}

class MainSurface extends SurfaceView implements SurfaceHolder.Callback {
	native void mty_set_surface(Surface surface);
	native void mty_surface_dims(int w, int h);
	native void mty_unset_surface();

	public MainSurface(Context context) {
		super(context);
        this.getHolder().addCallback(this);
	}

    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
		mty_surface_dims(w, h);
    }

    public void surfaceCreated(SurfaceHolder holder) {
		mty_set_surface(holder.getSurface());
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
		mty_unset_surface();
    }
}

public class MainActivity extends AppCompatActivity {
	static {
		System.loadLibrary("merton");
	}

	native void mty_global_init();

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// One time init
		if (savedInstanceState == null) {
			mty_global_init();

			// Runs the main function on a thread
			AppThread thread = new AppThread();
			thread.start();
		}

		int uiFlag = View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
			| View.SYSTEM_UI_FLAG_LAYOUT_STABLE
			| View.SYSTEM_UI_FLAG_LOW_PROFILE
			| View.SYSTEM_UI_FLAG_FULLSCREEN
			| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
			| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

		getWindow().getDecorView().setSystemUiVisibility(uiFlag);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

		ViewGroup vg = findViewById(android.R.id.content);
		ViewGroup.LayoutParams params = vg.getLayoutParams();

		MainSurface surface = new MainSurface(this.getApplicationContext());

		this.addContentView(surface, params);
	}
}
