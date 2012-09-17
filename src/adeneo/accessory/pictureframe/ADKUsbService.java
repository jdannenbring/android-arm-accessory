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

import android.app.IntentService;
import android.content.Intent;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.util.Log;

public class ADKUsbService extends IntentService {
	// load the library for handling libusb operations
	static {
		System.loadLibrary("adk-service");
	}
	
	public static final String TAG = "ADKUsbService";
	private static final int SAMPLE_RATE = 44100;
	private static final int BUFFER_SIZE = 70560;
	
	public ADKUsbService() {
		super("ADKUsbService");
	}
	
	private AudioTrack atrack;
	
	/* Native Declarations */
	private native void write();
	private native void killServiceThread();
	private native int setupISO();
	private native void monitorCommands();
	
	/* functions called by native adk-service.c */
	public int nextCmd()
	{
		Log.d(TAG, "NextCmd called from ADKUsbService");
		sendBroadcast(new Intent(ADKPictureFrame.NEXT_SLIDE));
		return 0;
	}
	
	public int prevCmd()
	{
		Log.d(TAG, "PrevCmd called from ADKUsbService");
		sendBroadcast(new Intent(ADKPictureFrame.PREV_SLIDE));
		return 0;
	}
	
	public int audioWrite(byte[] audioData, int offsetInBytes, int sizeInBytes)
	{
		return atrack.write(audioData, offsetInBytes, sizeInBytes);
	}
	
	public void updateAudioMetadata(String artist, String album, String track)
	{
		Log.d(TAG, "updating metadata: " + artist  + ":" + album + ":" + track);
		Intent metadata = new Intent(ADKPictureFrame.UPDATE_METADATA);
		metadata.putExtra(ADKPictureFrame.ARTIST_METADATA, artist);
		metadata.putExtra(ADKPictureFrame.ALBUM_METADATA, album);
		metadata.putExtra(ADKPictureFrame.TRACK_METADATA, track);
		sendBroadcast(metadata);
	}

	/* Service receives intent to start from ADKPictureFrame activity */
	@Override
	protected void onHandleIntent(Intent intent) {
		Log.i(TAG, "ADKUsbService started with : " + intent.getAction());
		if (intent.getAction().equals(ADKPictureFrame.AUDIO_STREAM_ATTACHED)) {
			/* Setup an AudioTrack */
        	atrack = new AudioTrack(AudioManager.STREAM_MUSIC, SAMPLE_RATE, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT, BUFFER_SIZE, AudioTrack.MODE_STREAM);
        	atrack.play();
        	
        	/* Create audio writing thread */
        	(new Thread(new Runnable() {
        		@Override
        		public void run() {
        			write();
        		}
        	})).start();
        	
			/* Handle the Audio Communication */
        	Log.d(TAG, "allocating isochronous transfer " + setupISO());
        	monitorCommands();
		}	
	}
	
	@Override
	public void onDestroy() {
        super.onDestroy();
		killServiceThread();
		atrack.stop();
    }
}
