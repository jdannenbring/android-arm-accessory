/*
 * This file is part of the Android Open Accessory (AOA) over USB example
 * program to support AOA1.0 and AOA2.0 on Linux/Android, ARM-based platforms
 *
 * Copyright (C) 2012 Jesse Dannenbring <jdannenbring@adeneo-embedded.com>
 *
 * Supports Audio streaming over USB (AOA2.0) and basic AOA1.0 custom command/control processing.
 * Displays currently playing music metadata, and presents control of picture slideshow.
 *
 * This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
package adeneo.accessory.pictureframe;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.ImageView;
import android.widget.TextView;


public class ADKPictureFrame extends Activity {
	// load the library for handling libusb operations
	static {
		System.loadLibrary("adk-service");
	}
	
	/* Receive intents from ADKUsbService and device detach event */
	private UsbServiceReceiver usbServiceReceiver;

	private static final String TAG = "USB_Accessory";
	private static final int ACCESSORY_VID = 0x18D1;
	private static final int PID_ACCESSORY_ONLY = 0x2D00;
	private static final int PID_ACCESSORY_ADB = 0x2D01;
	private static final int PID_ACCESSORY_ADB_AUDIO = 0x2D05;
	
	/* String definitions for Intent communication with ADKUsbService */
	public static final String NEXT_SLIDE = "android.arm.accessory.NEXT_SLIDE";
	public static final String PREV_SLIDE = "android.arm.accessory.PREV_SLIDE";
	public static final String UPDATE_METADATA = "android.arm.accessory.UPDATE_METADATA";
	public static final String ARTIST_METADATA = "android.arm.accessory.ARTIST_METADATA";
	public static final String ALBUM_METADATA = "android.arm.accessory.ALBUM_METADATA";
	public static final String TRACK_METADATA = "android.arm.accessory.TRACK_METADATA";
	public static final String AUDIO_STREAM_ATTACHED = "adeneo.accessory.pictureframe.action.AUDIO_STREAM_ATTACHED";
	
	private ImageView iv = null;
	private int curr_image = 0;
	private Intent service;
	
	/* Native Declarations */
	private native int checkAccessorySupport(
			String manufacturer,
			String modelName,
			String description,
			String version,
			String uri,
			String serialNumber,
			int vid, int pid);
	private native int connectToAccessory(int vid, int pid);
	
	/* displays next image in slideshow */
	public void nextImage() {
		switch(curr_image) {
		case 0:
			iv.setImageResource(R.drawable.pic2);
			break;
		case 1:
			iv.setImageResource(R.drawable.pic3);
			break;
		case 2:
			iv.setImageResource(R.drawable.pic4);
			break;
		case 3:
			iv.setImageResource(R.drawable.pic1);
			break;
		}
		curr_image = (curr_image + 1) % 4; 
	}
	
	/* displays previous image in slideshow */
	public void prevImage() {
		switch(curr_image) {
		case 0:
			iv.setImageResource(R.drawable.pic4);
			break;
		case 1:
			iv.setImageResource(R.drawable.pic1);
			break;
		case 2:
			iv.setImageResource(R.drawable.pic2);
			break;
		case 3:
			iv.setImageResource(R.drawable.pic3);
			break;
		}
		curr_image = (curr_image + 3) % 4; 
	}
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        iv = (ImageView) findViewById(R.id.imageView1);
        
        /* Check if USB connection started the app */
        Intent intent = getIntent();
        UsbDevice device = (UsbDevice) intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
        String action = intent.getAction();
        int vid, pid;
        
        /* USB device Attached */
        if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(action)) {
        	vid = device.getVendorId();
        	pid = device.getProductId();
        	if ((vid == ACCESSORY_VID)) {
        		switch(pid) {
        		case PID_ACCESSORY_ONLY:
        		case PID_ACCESSORY_ADB:
        		case PID_ACCESSORY_ADB_AUDIO:
        			Log.i(TAG, "Android device attached in Accessory Mode");
        			connectToAccessory(vid, pid);
        			
        			/* 
        	         * Register receiver to allow UsbService to update the UI 
        	         * when we receive commands over AOA
        	         */
        	        usbServiceReceiver = new UsbServiceReceiver();
        	        IntentFilter intentFilter = new IntentFilter(NEXT_SLIDE);
        	        intentFilter.addAction(PREV_SLIDE);
        	        intentFilter.addAction(UPDATE_METADATA);
        	        intentFilter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        	        registerReceiver(usbServiceReceiver, intentFilter);
        			
        			/* Allow the service to handle the audio communication */
        			service = new Intent(this, ADKUsbService.class);
        			service.setAction(AUDIO_STREAM_ATTACHED);
        			startService(service);
        			break;
    			default:
    				Log.d(TAG, "Checking usb device for accessory capabilities");
    	        	checkAccessorySupport(
    	            		"Freescale",
    	            		"iMX6Q",
    	            		"Description",
    	            		"SabreLite",
    	            		"http://www.adeneo-embedded.com",
    	            		"2254711SerialNo.",
    	            		vid, pid);
    	        	finish();
    				break;
        		}
        	} else {
        		Log.d(TAG, "Checking usb device for accessory capabilities");
	        	checkAccessorySupport(
	            		"Freescale",
	            		"iMX6Q",
	            		"Description",
	            		"SabreLite",
	            		"http://www.adeneo-embedded.com",
	            		"2254711SerialNo.",
	            		vid, pid);
	        	finish();
        	}
        } else {
        	vid = 0x18d1;
        	pid = 0x4e42;
    		Log.d(TAG, "Checking usb device for accessory capabilities");
        	int ret = checkAccessorySupport(
            		"Freescale",
            		"iMX6Q",
            		"Cortex-A9",
            		"SabreLite",
            		"http://www.adeneo-embedded.com",
            		"FakeSerial01010101",
            		vid, pid);
        	if (ret == 0) {
	        	Log.i(TAG, "Android device attached in Accessory Mode");
	        	pid = 0x2d05;
	        	Log.d(TAG, getFilesDir().getAbsolutePath());
				if (connectToAccessory(vid, pid) != 0)
					Log.e(TAG, "Error connecting to accessory");
        	} else {
        		Log.e(TAG, "Error starting the device in accessory mode");
        	}
        }
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (usbServiceReceiver != null)
        	unregisterReceiver(usbServiceReceiver);
        
        service = new Intent(this, ADKUsbService.class);
        stopService(service);
    }
    
    /* Broadcast Receiver for receiving updates from UsbService */
    private class UsbServiceReceiver extends BroadcastReceiver {
    	@Override
    	public void onReceive(Context context, Intent intent) {
    		if (intent.getAction().equals(NEXT_SLIDE)) {
    			Log.d(TAG, "Activity received NextSlide command");
    			nextImage();
    		} else if (intent.getAction().equals(PREV_SLIDE)) {
    			Log.d(TAG, "Activity received PrevSlide command");
    			prevImage();
    		} else if (intent.getAction().equals(UPDATE_METADATA)) {
    			Log.d(TAG, "Activity received UpdateMetadata command");
    			String artist_meta = intent.getStringExtra(ARTIST_METADATA);
    			String album_meta = intent.getStringExtra(ALBUM_METADATA);
    			String track_meta = intent.getStringExtra(TRACK_METADATA);
    			Log.d(TAG, "artist = " + artist_meta);
    			Log.d(TAG, "album = " + album_meta);
    			Log.d(TAG, "track = " + track_meta);
    			
    			/* update the audio metadata */
    			TextView artist = (TextView) findViewById(R.id.Artist);
    			TextView album = (TextView) findViewById(R.id.Album);
    			TextView track = (TextView) findViewById(R.id.Track);
    			if (!(artist_meta.equals("null")))
    				artist.setText(artist_meta);
    			if (!(album_meta.equals("null")))
    				album.setText(album_meta);
    			if (!(track_meta.equals("null")))
    				track.setText(track_meta);
    		} else if(UsbManager.ACTION_USB_DEVICE_DETACHED.equals(intent.getAction())) {
            	Log.d(TAG, "received the usb device detached message");
            	finish();
            } 
    	}
    }
}
