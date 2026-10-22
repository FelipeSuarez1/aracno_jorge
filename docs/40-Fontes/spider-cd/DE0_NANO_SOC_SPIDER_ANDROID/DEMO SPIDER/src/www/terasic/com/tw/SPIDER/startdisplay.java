package www.terasic.com.tw.SPIDER;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import java.text.SimpleDateFormat;

public class startdisplay extends Activity{
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.startdisplay); 
		 new Thread(new Runnable(){

	            @Override
	            public void run() {
	                // TODO Auto-generated method stub
	                try {
	                    Thread.sleep(3000);//這邊可以做你想預先載入的資料
	                    startActivity(new Intent().setClass(startdisplay.this, BluetoothChat.class));
	                    startdisplay.this.finish(); 
	                } catch (InterruptedException e) {
	                    // TODO Auto-generated catch block
	                    e.printStackTrace();
	                }
	            }
	            
	        }).start();
		/*
		Button button = (Button)findViewById(R.id.button01); 
		button.setOnClickListener(new Button.OnClickListener(){
		@Override
		public void onClick(View v) {
		// TODO Auto-generated method stub 
		Intent intent = new Intent();
		intent.setClass(startdisplay.this, BluetoothChat.class);
		startActivity(intent); 
		startdisplay.this.finish(); 
		}
		});
		}
		*/

	}
}
