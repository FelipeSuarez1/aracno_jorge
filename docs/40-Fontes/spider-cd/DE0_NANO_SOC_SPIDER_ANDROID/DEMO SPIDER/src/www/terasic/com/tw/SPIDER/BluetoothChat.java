/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package www.terasic.com.tw.SPIDER;




import android.annotation.SuppressLint;
import android.app.ActionBar;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Intent;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.media.AudioManager;
import android.media.SoundPool;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.inputmethod.EditorInfo;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

/**
 * This is the main Activity that displays the current chat session.
 */
public class BluetoothChat extends Activity implements SensorEventListener {
    // Debugging
    private static final String TAG = "BluetoothChat";
    private static final boolean D = true;

    // Message types sent from the BluetoothChatService Handler
    public static final int MESSAGE_STATE_CHANGE = 1;
    public static final int MESSAGE_READ = 2;
    public static final int MESSAGE_WRITE = 3;
    public static final int MESSAGE_DEVICE_NAME = 4;
    public static final int MESSAGE_TOAST = 5;

    // Key names received from the BluetoothChatService Handler
    public static final String DEVICE_NAME = "device_name";
    public static final String TOAST = "toast";

    // Intent request codes
    private static final int REQUEST_CONNECT_DEVICE_SECURE = 1;
    private static final int REQUEST_CONNECT_DEVICE_INSECURE = 2;
    private static final int REQUEST_ENABLE_BT = 3;

    // Layout Views
    private ListView mConversationView;

    //Control Button
    private ImageView DownMove;
    private ImageView UpMove;
    private ImageView RightMove;
    private ImageView LeftMove;
    private ImageView ResetMove;
    //Mode Switch
    private ImageView ModeSwitch;
    int     Mode = 0;
    //demo button
    private ImageView Dance;
    //gyro
    float[] vector= new float[3];
    private TextView Gyro_X, Gyro_Y, Gyro_Z;
    private SensorManager mSensorManager;
    private Sensor mSensor;
    boolean Once_Flag=false;
    
    //connect image
    private ImageView mConnectState;
    
    // Name of the connected device
    private String mConnectedDeviceName = null;
    // Array adapter for the conversation thread
    private ArrayAdapter<String> mConversationArrayAdapter;
    // String buffer for outgoing messages
    private StringBuffer mOutStringBuffer;
    // Local Bluetooth adapter
    private BluetoothAdapter mBluetoothAdapter = null;
    // Member object for the chat services
    private BluetoothChatService mChatService = null;
    //play sound
    private SoundPool soundPool;
    private int vpress;
    private int vbrake;
    private int vdance;

    @SuppressLint("NewApi")
	private void SetSensor() {
        // TODO Auto-generated method stub
    	Gyro_X = (TextView) findViewById(R.id.x_vector);
        Gyro_Y = (TextView) findViewById(R.id.y_vector);
        Gyro_Z = (TextView) findViewById(R.id.z_vector);
        mSensorManager = (SensorManager) getSystemService(SENSOR_SERVICE);
        mSensor = mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        mSensorManager.registerListener(this, mSensor, SensorManager.SENSOR_DELAY_NORMAL);
    }
    
    
    @SuppressLint("NewApi")
	@Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if(D) Log.e(TAG, "+++ ON CREATE +++");

        // Set up the window layout
        setContentView(R.layout.main);
        //play music
        soundPool = new SoundPool(1, AudioManager.STREAM_MUSIC, 5);
        vpress = soundPool.load(this, R.drawable.pressvoice, 1); 
        vbrake = soundPool.load(this, R.drawable.brake, 1); 
        vdance = soundPool.load(this, R.drawable.vdemo, 1); 
        //Control Button
        DownMove = (ImageView) findViewById(R.id.imageViewdown);
        UpMove = (ImageView) findViewById(R.id.imageViewleft);
        RightMove = (ImageView) findViewById(R.id.imageViewright);
        UpMove = (ImageView) findViewById(R.id.imageViewup);
        ResetMove = (ImageView) findViewById(R.id.imageViewreset);
        //Mode Switch
        ModeSwitch = (ImageView) findViewById(R.id.imageViewMSwitch);
        //Dance
        Dance = (ImageView) findViewById(R.id.imageViewdance);
        
        //gyro
        SetSensor();
        //SeekBar
        SeekBar seekBar = (SeekBar)findViewById(R.id.seekBar);  
        final TextView seekBarValue = (TextView)findViewById(R.id.seekbarvalue); 
        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){  
        int Gprogress = 0;
        @Override  
        public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {  
        // TODO Auto-generated method stub  
        Gprogress = progress;
        seekBarValue.setText("    Speed " + String.valueOf(progress));
        }  
  
        @Override  
        public void onStartTrackingTouch(SeekBar seekBar) {  
        // TODO Auto-generated method stub  
        }  
  
        @Override  
        public void onStopTrackingTouch(SeekBar seekBar) {  
        // TODO Auto-generated method stub  
        	
        seekBarValue.setText("    Speed " +String.valueOf(Gprogress)); 
        sendMessage("ATSP="+Gprogress+"\r");
        }  
        });  
      //connect state
        mConnectState = (ImageView) findViewById(R.id.imageViewunconnect);
        
        // Get local Bluetooth adapter
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();

        // If the adapter is null, then Bluetooth is not supported
        if (mBluetoothAdapter == null) {
            Toast.makeText(this, "Bluetooth is not available", Toast.LENGTH_LONG).show();
            finish(); 
            return;
        }
    }

    @SuppressLint("NewApi")
	@Override
    public void onStart() {
        super.onStart();
        if(D) Log.e(TAG, "++ ON START ++");

        // If BT is not on, request that it be enabled.
        // setupChat() will then be called during onActivityResult
        if (!mBluetoothAdapter.isEnabled()) {
            Intent enableIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivityForResult(enableIntent, REQUEST_ENABLE_BT);
        // Otherwise, setup the chat session
        } else {
            if (mChatService == null) setupChat();
        }
    }

    @Override
    public synchronized void onResume() {
        super.onResume();
        //gyro
        mSensorManager.registerListener(this, mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER), SensorManager.SENSOR_DELAY_GAME);
        
        if(D) Log.e(TAG, "+ ON RESUME +");

        // Performing this check in onResume() covers the case in which BT was
        // not enabled during onStart(), so we were paused to enable it...
        // onResume() will be called when ACTION_REQUEST_ENABLE activity returns.
        if (mChatService != null) {
            // Only if the state is STATE_NONE, do we know that we haven't started already
            if (mChatService.getState() == BluetoothChatService.STATE_NONE) {
              // Start the Bluetooth chat services
              mChatService.start();
            }
        }
    }

    @SuppressLint("NewApi")
	private void setupChat() {
        Log.d(TAG, "setupChat()");

        // Initialize the array adapter for the conversation thread
        mConversationArrayAdapter = new ArrayAdapter<String>(this, R.layout.message);
        mConversationView = (ListView) findViewById(R.id.in);
        mConversationView.setAdapter(mConversationArrayAdapter);
        
        ModeSwitch = (ImageView) findViewById(R.id.imageViewMSwitch);
        ModeSwitch.setOnClickListener(new OnClickListener(){
        	 public void onClick(View v) {
        		 if(Mode==0)
        		 {
        			 Mode = 1;
        			 ModeSwitch.setImageDrawable(getResources().getDrawable(R.drawable.gmod));
        		 }
        		 else
        		 {
        			 Mode = 0;
        			 ModeSwitch.setImageDrawable(getResources().getDrawable(R.drawable.kmod));
        		 }
        	 }
        });
        Dance = (ImageView) findViewById(R.id.imageViewdance);
        Dance.setOnClickListener(new OnClickListener(){
        	 public void onClick(View v) {
        		 soundPool.play(vdance, 1.0F, 1.0F, 0, 0, 1.0F);
        		 sendMessage("ATALL\r");
        	 }
        });
        
        DownMove = (ImageView) findViewById(R.id.imageViewdown);
        DownMove.setOnClickListener(new OnClickListener(){
        	 public void onClick(View v) {
        		 soundPool.play(vpress, 1.0F, 1.0F, 0, 0, 1.0F);
        		 sendMessage("ATBW\r");
        	 }
        });
        UpMove = (ImageView) findViewById(R.id.imageViewup);
        UpMove.setOnClickListener(new OnClickListener(){
        	 public void onClick(View v) {
        		 soundPool.play(vpress, 1.0F, 1.0F, 0, 0, 1.0F);
        		 sendMessage("ATFW\r");
        	 }
        });
        RightMove = (ImageView) findViewById(R.id.imageViewright);
        RightMove.setOnClickListener(new OnClickListener(){
        	 public void onClick(View v) {
        		 soundPool.play(vpress, 1.0F, 1.0F, 0, 0, 1.0F);
        		 sendMessage("ATTR\r");
        	 }
        });
        LeftMove = (ImageView) findViewById(R.id.imageViewleft);
        LeftMove.setOnClickListener(new OnClickListener(){
        	 public void onClick(View v) {
        		 soundPool.play(vpress, 1.0F, 1.0F, 0, 0, 1.0F);
        		 sendMessage("ATTL\r");
        	 }
        });
        ResetMove = (ImageView) findViewById(R.id.imageViewreset);
        ResetMove.setOnClickListener(new OnClickListener(){
        	 public void onClick(View v) {
        		 soundPool.play(vbrake, 1.0F, 1.0F, 0, 0, 1.0F);
        		 sendMessage("ATST\r");
        	 }
        });
        
        // Initialize the BluetoothChatService to perform bluetooth connections
        mChatService = new BluetoothChatService(this, mHandler);

        // Initialize the buffer for outgoing messages
        mOutStringBuffer = new StringBuffer("");
    }

    @Override
    public synchronized void onPause() {
        super.onPause();
        //gyro
        mSensorManager.unregisterListener(this);
        if(D) Log.e(TAG, "- ON PAUSE -");
    }

    @Override
    public void onStop() {
        super.onStop();
        if(D) Log.e(TAG, "-- ON STOP --");
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // Stop the Bluetooth chat services
        if (mChatService != null) mChatService.stop();
        if(D) Log.e(TAG, "--- ON DESTROY ---");
    }

    @SuppressLint("NewApi")
	private void ensureDiscoverable() {
        if(D) Log.d(TAG, "ensure discoverable");
        if (mBluetoothAdapter.getScanMode() !=
            BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE) {
            Intent discoverableIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
            discoverableIntent.putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION, 300);
            startActivity(discoverableIntent);
        }
    }
    @SuppressLint("NewApi")
 	@Override
     public void onSensorChanged(SensorEvent event) {
         // TODO Auto-generated method stub
     	for(int i=0;i<3;i++)
     		vector[i] = event.values[i];
     	
         Gyro_X.setText("X : " + String.format("%.3f",vector[0]));
         Gyro_Y.setText("Y : " + String.format("%.3f",vector[1]));
         Gyro_Z.setText("Z : " + String.format("%.3f",vector[2]));
         if(Mode==1)
         {
         	if((int)vector[0] >  4)
         	{
         		sendMessage("ATTTL\r");
         	}
         	else if((int)vector[0] < -4)
         	{
         		sendMessage("ATTTR\r");
         	}
         	else if((int)vector[1] < -4)
         	{
         		sendMessage("ATTTF\r");
         	}
         	else if((int)vector[1] > 4)
         	{
         		sendMessage("ATTTB\r");
         	}
         	else
         		sendMessage("ATTTN\r");
         }
     }
    /**
     * Sends a message.
     * @param message  A string of text to send.
     */
    private void sendMessage(String message) {
        // Check that we're actually connected before trying anything
        if (mChatService.getState() != BluetoothChatService.STATE_CONNECTED) {
            Toast.makeText(this, R.string.not_connected, Toast.LENGTH_SHORT).show();
            return;
        }

        // Check that there's actually something to send
        if (message.length() > 0) {
            // Get the message bytes and tell the BluetoothChatService to write
            byte[] send = message.getBytes();
            mChatService.write(send);

            // Reset out string buffer to zero and clear the edit text field
            mOutStringBuffer.setLength(0);
        }
    }

    // The action listener for the EditText widget, to listen for the return key
    @SuppressLint("NewApi")
	private TextView.OnEditorActionListener mWriteListener =
        new TextView.OnEditorActionListener() {
        public boolean onEditorAction(TextView view, int actionId, KeyEvent event) {
            // If the action is a key-up event on the return key, send the message
            if (actionId == EditorInfo.IME_NULL && event.getAction() == KeyEvent.ACTION_UP) {
                String message = view.getText().toString();
                sendMessage(message);
            }
            if(D) Log.i(TAG, "END onEditorAction");
            return true;
        }
    };

    @SuppressLint("NewApi")
	private final void setStatus(int resId) {
        final ActionBar actionBar = getActionBar();
        if(actionBar!= null)
        actionBar.setSubtitle(resId);
    }

    @SuppressLint("NewApi")
	private final void setStatus(CharSequence subTitle) {
        final ActionBar actionBar = getActionBar();
        if(actionBar!= null)
        actionBar.setSubtitle(subTitle);
    }

    // The Handler that gets information back from the BluetoothChatService
    private final Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
            case MESSAGE_STATE_CHANGE:
                if(D) Log.i(TAG, "MESSAGE_STATE_CHANGE: " + msg.arg1);
                switch (msg.arg1) {
                case BluetoothChatService.STATE_CONNECTED:
                    setStatus(getString(R.string.title_connected_to, mConnectedDeviceName));
                    mConversationArrayAdapter.clear();
                    mConnectState.setImageDrawable(getResources().getDrawable(R.drawable.connect));
                    break;
                case BluetoothChatService.STATE_CONNECTING:
                    setStatus(R.string.title_connecting);
                    break;
                case BluetoothChatService.STATE_LISTEN:
                case BluetoothChatService.STATE_NONE:
                	mConnectState.setImageDrawable(getResources().getDrawable(R.drawable.unconnect));
                    setStatus(R.string.title_not_connected);
                    break;
                }
                break;
            case MESSAGE_WRITE:
                byte[] writeBuf = (byte[]) msg.obj;
                // construct a string from the buffer
                String writeMessage = new String(writeBuf);
                mConversationArrayAdapter.add("CMD:  " + writeMessage);
                break;
            case MESSAGE_READ:
                byte[] readBuf = (byte[]) msg.obj;
                // construct a string from the valid bytes in the buffer
                String readMessage = new String(readBuf, 0, msg.arg1);
                mConversationArrayAdapter.add(mConnectedDeviceName+":  " + readMessage);
                break;
            case MESSAGE_DEVICE_NAME:
                // save the connected device's name
                mConnectedDeviceName = msg.getData().getString(DEVICE_NAME);
                Toast.makeText(getApplicationContext(), "Connected to "
                               + mConnectedDeviceName, Toast.LENGTH_SHORT).show();
                break;
            case MESSAGE_TOAST:
                Toast.makeText(getApplicationContext(), msg.getData().getString(TOAST),
                               Toast.LENGTH_SHORT).show();
                break;
            }
        }
    };

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if(D) Log.d(TAG, "onActivityResult " + resultCode);
        switch (requestCode) {
        case REQUEST_CONNECT_DEVICE_SECURE:
            // When DeviceListActivity returns with a device to connect
            if (resultCode == Activity.RESULT_OK) {
                connectDevice(data, true);
            }
            break;
        case REQUEST_CONNECT_DEVICE_INSECURE:
            // When DeviceListActivity returns with a device to connect
            if (resultCode == Activity.RESULT_OK) {
                connectDevice(data, false);
            }
            break;
        case REQUEST_ENABLE_BT:
            // When the request to enable Bluetooth returns
            if (resultCode == Activity.RESULT_OK) {
                // Bluetooth is now enabled, so set up a chat session
                setupChat();
            } else {
                // User did not enable Bluetooth or an error occurred
                Log.d(TAG, "BT not enabled");
                Toast.makeText(this, R.string.bt_not_enabled_leaving, Toast.LENGTH_SHORT).show();
                finish();
            }
        }
    }

    @SuppressLint("NewApi")
	private void connectDevice(Intent data, boolean secure) {
        // Get the device MAC address
        String address = data.getExtras()
            .getString(DeviceListActivity.EXTRA_DEVICE_ADDRESS);
        // Get the BluetoothDevice object
        BluetoothDevice device = mBluetoothAdapter.getRemoteDevice(address);
        // Attempt to connect to the device
        mChatService.connect(device, secure);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.option_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        Intent serverIntent = null;
        switch (item.getItemId()) {
        case R.id.secure_connect_scan:
            // Launch the DeviceListActivity to see devices and do scan
            serverIntent = new Intent(this, DeviceListActivity.class);
            startActivityForResult(serverIntent, REQUEST_CONNECT_DEVICE_SECURE);
            return true;
        /*case R.id.insecure_connect_scan:
            // Launch the DeviceListActivity to see devices and do scan
            serverIntent = new Intent(this, DeviceListActivity.class);
            startActivityForResult(serverIntent, REQUEST_CONNECT_DEVICE_INSECURE);
            return true;*/
        case R.id.discoverable:
            // Ensure this device is discoverable by others
            ensureDiscoverable();
            return true;
        }
        return false;
    }


	@Override
	public void onAccuracyChanged(Sensor sensor, int accuracy) {
		// TODO Auto-generated method stub
		
	}

}
