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
#include <stdio.h>
#include <libusb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <jni.h>

#include <android/log.h>

#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "adk-service", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , "adk-service", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO   , "adk-service", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN   , "adk-service", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR  , "adk-service", __VA_ARGS__)

#include "buffer.h"

#define ACCESSORY_VID 0x18D1
#define ACCESSORY_PID PID_ACCESSORY_ADB_AUDIO
#define PID_ACCESSORY_ONLY 0x2D00
#define PID_ACCESSORY_ADB 0x2D01
#define PID_AUDIO 0x2D02
#define PID_AUDIO_ADB 0x2D03
#define PID_ACCESSORY_AUDIO 0x2D04
#define PID_ACCESSORY_ADB_AUDIO 0x2D05

#define AOA_GET_PROTOCOL		51
#define AOA_SEND_IDENT			52
#define AOA_START_ACCESSORY		53
#define AOA_AUDIO_SUPPORT		58

#define CMD_BUFFER_SIZE 500
#define METADATA_LENGTH 100

/*
 * Nexus 7 endpoints and interfaces.
 * This needs to be dynamically chosen
 * once concept is working.
 */
#define NEXUS7_AUDIO_INTERFACE	2
#define NEXUS7_AUDIO_ALT_SETTING	1
#define NEXUS7_ISO_ENDPOINT	0x83
#define AUDIO_NUM_ISO_PACKETS 400

/* These should be found by matching iInterface w/ Android Accessory Interface */
#define BULK_ENDPOINT_IN	0x81
#define BULK_ENDPOINT_OUT	0x02

// internal buffer for thread_write
#define BUFFER_FRAMES 2205

// circular buffer size
#define BUFFER_SIZE 576000
#define SLEEP_TIME	2000000
#define MIN_TRANSFER_SIZE 4410

// 16-bit PCM, Stereo
#define BITS_PER_FRAME 32

static int chk(int code);

static struct libusb_device_handle* handle;
static struct libusb_device_handle* accessory_handle;
static FILE *fout;
// Global exit flag
static int quit = 0;
// Handle to the circular buffer used by read&write threads
static buffer_handle_t *buffer = NULL;

int 
Java_adeneo_accessory_pictureframe_ADKPictureFrame_checkAccessorySupport(JNIEnv* env, jobject thiz,
	jstring jmanufacturer,
	jstring jmodelName,
	jstring jdescription,
	jstring jversion,
	jstring juri,
	jstring jserialNumber,
	int vid,
	int pid){

	unsigned char ioBuffer[2];
	int aoa_version;
	int response;
	int tries = 5;
	int audio_mode = 1;
	
	const char* manufacturer = (*env)->GetStringUTFChars(env, jmanufacturer, NULL);
	const char* modelName = (*env)->GetStringUTFChars(env, jmodelName, NULL);
	const char* description = (*env)->GetStringUTFChars(env, jdescription, NULL);
	const char* version = (*env)->GetStringUTFChars(env, jversion, NULL);
	const char* uri = (*env)->GetStringUTFChars(env, juri, NULL);
	const char* serialNumber = (*env)->GetStringUTFChars(env, jserialNumber, NULL);
							
	libusb_init(NULL);
	if((handle = libusb_open_device_with_vid_pid(NULL, vid, pid)) == NULL){
		return -1;
	} else {
		libusb_claim_interface(handle, 0);
	}

	/* See if the device is capable of AOA version 1/2 */
	response = libusb_control_transfer(
		handle, //handle
		LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR, //bmRequestType
		AOA_GET_PROTOCOL, //bRequest
		0, //wValue
		0, //wIndex
		ioBuffer, //data
		2, //wLength
        0 //timeout
	);

	if (response < 0) {
		LOGE("USB Device is not capable of AOA");
		return chk(response);
	}

	aoa_version = ioBuffer[1] << 8 | ioBuffer[0];
	LOGI("Version Device Code: %d", aoa_version);
	if (aoa_version < 1 || aoa_version > 2) {
		LOGE("USB Device is not capable of AOA 1.0 or 2.0");
		return -1;
	}
	
	usleep(10000); /* Some Android devices require a waiting period between transfer calls */

	response = libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_SEND_IDENT,0,0,(char*)manufacturer,strlen(manufacturer),0);
	if(response < 0)
		return chk(response);
	usleep(1000);
	response = libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_SEND_IDENT,0,1,(char*)modelName,strlen(modelName)+1,0);
	if(response < 0)
		return chk(response);
	usleep(1000);
	response = libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_SEND_IDENT,0,2,(char*)description,strlen(description)+1,0);
	if(response < 0)
		return chk(response);
	usleep(1000);
	response = libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_SEND_IDENT,0,3,(char*)version,strlen(version)+1,0);
	if(response < 0)
		return chk(response);
	usleep(1000);
	response = libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_SEND_IDENT,0,4,(char*)uri,strlen(uri)+1,0);
	if(response < 0)
		return chk(response);
	usleep(1000);
	response = libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_SEND_IDENT,0,5,(char*)serialNumber,strlen(serialNumber)+1,0);
	if(response < 0)
		return chk(response);
	usleep(1000);
	response = libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_AUDIO_SUPPORT,audio_mode,0,0,0,0);
	if(response < 0)
		return chk(response);
	usleep(1000);

	LOGD("Accessory Identification sent");

	response = libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_START_ACCESSORY,0,0,NULL,0,0);
	if(response < 0)
		return chk(response);

	LOGD("Attempted to put device into accessory mode");

	if(handle != NULL) {
		libusb_release_interface (handle, 0);
		libusb_close(handle);
		handle = NULL;
	}

	(*env)->ReleaseStringUTFChars(env, jmanufacturer, manufacturer);
	(*env)->ReleaseStringUTFChars(env, jmodelName, modelName);
	(*env)->ReleaseStringUTFChars(env, jdescription, description);
	(*env)->ReleaseStringUTFChars(env, jversion, version);
	(*env)->ReleaseStringUTFChars(env, juri, uri);
	(*env)->ReleaseStringUTFChars(env, jserialNumber, serialNumber);
	
	return 0;
}

int
Java_adeneo_accessory_pictureframe_ADKPictureFrame_connectToAccessory(JNIEnv* env,jobject thiz, int vid, int pid){
	LOGD("Connecting to device 0x%x:0x%x after requesting accessory mode", vid, pid);
	sleep(1);
	accessory_handle = libusb_open_device_with_vid_pid(NULL, vid, pid);
	if(accessory_handle == NULL){
		return -1;
	}

	/* If audio is supported setup buffer */
	switch (pid) {
	case PID_AUDIO:
	case PID_AUDIO_ADB:
	case PID_ACCESSORY_AUDIO:
	case PID_ACCESSORY_ADB_AUDIO:
		// Initialize the circular buffer
		buffer = buffer_create(BUFFER_SIZE);
		if (!buffer) {
			LOGE("Failed to allocate buffer");
			return -1;
		}
		break;
	}

	sleep(1);
	if (chk(libusb_claim_interface(accessory_handle, 0)) != 0)
		return -1;

	if (chk(libusb_claim_interface(accessory_handle, NEXUS7_AUDIO_INTERFACE)) != 0)
		return -1;

	LOGI("Accessory usb interface claimed, ready for data transfer");

	return 0;
}

/* allows write() method to exit */
void
Java_adeneo_accessory_pictureframe_ADKUsbService_killServiceThread(JNIEnv* env,jobject jObj)
{
	quit = 1;
	return;
}

/* Audio transfer thread */
void
Java_adeneo_accessory_pictureframe_ADKUsbService_write(JNIEnv* env,jobject jObj)
{
	int buffer_size = BUFFER_FRAMES * BITS_PER_FRAME;

	u_char *data = (u_char *)malloc(buffer_size);
	if (!data)
	{
		LOGE("ERROR: failed to allocate memory");
		goto end;
	}
	memset(data, 0, buffer_size);

	LOGD("AudioWrite thread buffer = %i bytes; BUFFER_SIZE = %i bytes; MIN_TRANSFER_SIZE = %i bytes", (BUFFER_FRAMES * BITS_PER_FRAME) / 4, BUFFER_SIZE, MIN_TRANSFER_SIZE);


	jclass jc = (*env)->GetObjectClass(env, jObj);
	jmethodID method = (*env)->GetMethodID(env, jc, "audioWrite", "([BII)I");
	/* create jbytearray of 0's size of buffer_size*/
	jbyteArray bArray = (*env)->NewByteArray(env, buffer_size);
	jboolean isCopy;
	void *raw_data = (*env)->GetPrimitiveArrayCritical(env, (jarray)bArray, &isCopy);
	memcpy(raw_data, data, buffer_size);
	(*env)->ReleasePrimitiveArrayCritical(env, (jarray)bArray, raw_data, 0);


	// This phase is important and will fill the output buffer.
	// If you do not preload, you will get underruns.
	// The value has been determined empirically.
	//
	LOGD("Preloading...");
	while(buffer_read_avail(buffer) <= 0) {
		(*env)->CallIntMethod(env, jObj, method, bArray, 0, buffer_size);
		usleep(SLEEP_TIME*4);
	}

	/* main audio write loop */
	while (!quit)
	{
		int count = buffer_read_avail(buffer);
		double percentage_full = count;
		percentage_full = (percentage_full / BUFFER_SIZE) * 100;

		if (count <= MIN_TRANSFER_SIZE){
			usleep(SLEEP_TIME);
			continue;
		}

		count = count > buffer_size ? buffer_size : count;
		// Read from circular buffer
		int done = 0;
		while (!quit) {
			done += buffer_read(buffer, data + done, count - done);

			if (done >= count) {
				break;
			}
			else {
				// Wait until we have something to read from the circular buffer
				// (i.e. the USB thread has written some data from the android device)
				usleep(SLEEP_TIME);
				LOGI("sleeping");
			}
		}

		/* convert buffer's u_char 'data' to jbytearray */
		raw_data = (*env)->GetPrimitiveArrayCritical(env, (jarray)bArray, &isCopy);
		memcpy(raw_data, data, buffer_size);
		(*env)->ReleasePrimitiveArrayCritical(env, (jarray)bArray, raw_data, 0);

		// Write to the audio device.
		int ret = (*env)->CallIntMethod(env, jObj, method, bArray, 0, count);

		if (ret < 0) {
			LOGE("ERROR: audioWrite() failed");
			goto end;
		}
	}

end:
	if (data) {
		free(data);
		data = NULL;
	}


	quit = 1;
	LOGD("audioWrite exiting...");
}

/*
 * Callback for Isochronous transfers (audio)
 */
static void audio_callback(struct libusb_transfer *transfer)
{
	int i;
	unsigned char *buf;
	int written;

	for (i = 0; i < AUDIO_NUM_ISO_PACKETS; i++) {
		if (transfer->iso_packet_desc[i].status == LIBUSB_TRANSFER_COMPLETED) {
			buf = libusb_get_iso_packet_buffer_simple(transfer, i);
			written = 0;
			while (1) {
				written += buffer_write(buffer, buf + written, transfer->iso_packet_desc[i].actual_length - written);

				if (written >= transfer->iso_packet_desc[i].actual_length) {
					break;
				}
			}
		} else {
			LOGE("Bad iso packet status: %i", transfer->iso_packet_desc[i].status);
		}
	}
	libusb_submit_transfer(transfer);
}

/*
 * This function performs Allocation, Filling & Submission of the
 * Isochronous usb transfer.
 * It also switches the Android device to use the interface's alt
 * setting
 */
int Java_adeneo_accessory_pictureframe_ADKUsbService_setupISO(JNIEnv* env,jobject jObj) {
	struct libusb_transfer *transfer = libusb_alloc_transfer(AUDIO_NUM_ISO_PACKETS); // 10 packets...
	int i, max_pkt_size;
	int ret = -1;

	if (!transfer) {
		LOGE("Error running libusb_alloc_transfer!");
		return -1;
	}

	if (accessory_handle == NULL) {
			return -1;
	}

	sleep(1);
	ret = libusb_set_interface_alt_setting(accessory_handle, NEXUS7_AUDIO_INTERFACE, NEXUS7_AUDIO_ALT_SETTING);
	if (ret != 0)
		LOGE("Error setting alt: %i", ret);
	else
		LOGD("Setting alt successful");

	sleep(1);
	max_pkt_size = libusb_get_max_packet_size(libusb_get_device(accessory_handle), NEXUS7_ISO_ENDPOINT);
	if(max_pkt_size <= 0)  {
		LOGE("Error getting max packet size");
		chk(max_pkt_size);
		return -1;
	} else {
		LOGD("Max Packet size for endpoint %i = %i", NEXUS7_ISO_ENDPOINT, max_pkt_size);
	}

	transfer->dev_handle = accessory_handle;
	transfer->endpoint = NEXUS7_ISO_ENDPOINT;
	transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
	transfer->timeout = 0; // no timeout

	transfer->length = AUDIO_NUM_ISO_PACKETS * max_pkt_size;
	transfer->buffer = malloc(transfer->length);
	transfer->callback = audio_callback;
	transfer->num_iso_packets = AUDIO_NUM_ISO_PACKETS;

	libusb_set_iso_packet_lengths(transfer, max_pkt_size);

	if (chk(libusb_submit_transfer(transfer)) != 0) {
		LOGE("Submit transfer failed");
		return -1;
	} else {
		return 0;
	}
}

/* ADK specific command parsing loop. Good place for libusb_handle_events() call */
void Java_adeneo_accessory_pictureframe_ADKUsbService_monitorCommands(JNIEnv* env,jobject jObj){
	u_char buffer[CMD_BUFFER_SIZE];
	int response = 0;
	static int num_bytes_xfer;
	int i;

	char *begin;
	int len;
	char artist[METADATA_LENGTH] = "";
	char album[METADATA_LENGTH] = "";
	char track[METADATA_LENGTH] = "";
	char isPlaying[10] = "";

	const char *PLAYSTATE_CHANGED = "com.android.music.playstatechanged";
	const char *META_CHANGED = "com.android.music.metachanged";
	const char *QUEUE_CHANGED = "com.android.music.queuechanged";
	const char *PLAYBACK_COMPLETE = "com.android.music.playbackcomplete";
	const char *DELIMITER = "/";

	while(1) {
		if(chk(libusb_handle_events(NULL)) != 0)
			break;

		response = libusb_bulk_transfer(accessory_handle, BULK_ENDPOINT_IN, buffer, CMD_BUFFER_SIZE, &num_bytes_xfer, 0);
		if(response < 0) {
			chk(response);
			break;
		} else {
			buffer[num_bytes_xfer] = 0;
			LOGD("Received: %s", buffer);

			/* Command Parsing & Response */
			jclass jc = (*env)->GetObjectClass(env, jObj);
			jmethodID method = 0;
			if (strcmp(buffer, "next") == 0) {
				LOGD("received Next slide command");
				method = (*env)->GetMethodID(env, jc, "nextCmd", "()I");
				if (method)
					(*env)->CallIntMethod(env, jObj, method, 0, 0, 0);
			} else if (strcmp(buffer, "prev") == 0) {
				LOGD("received Prev slide command");
				method = (*env)->GetMethodID(env, jc, "prevCmd", "()I");
				if (method)
					(*env)->CallIntMethod(env, jObj, method, 0, 0, 0);
			} else if (strncmp(buffer, PLAYSTATE_CHANGED, 23) == 0 ||
					strncmp(buffer, META_CHANGED, 23) == 0 ||
					strncmp(buffer, QUEUE_CHANGED, 23) == 0 ||
					strncmp(buffer, PLAYBACK_COMPLETE, 23) == 0) {

				// clear previous strings
				memset(artist, 0, sizeof(artist));
				memset(album, 0, sizeof(album));
				memset(track, 0, sizeof(track));
				memset(isPlaying, 0, sizeof(isPlaying));

				// parse the music metadata using / as the delimiter
				LOGD("received music metadata");
				begin = strpbrk(buffer, DELIMITER);
				begin++;
				len = strcspn(begin, DELIMITER);

				// artist
				strncpy(artist, begin, len);
				LOGD("artist = %s", artist);
				begin = strpbrk(begin, DELIMITER);
				begin++;
				len = strcspn(begin, DELIMITER);
				// album
				strncpy(album, begin, len);
				LOGD("album = %s", album);
				begin = strpbrk(begin, DELIMITER);
				begin++;
				len = strcspn(begin, DELIMITER);
				// track
				strncpy(track, begin, len);
				LOGD("track = %s", track);
				begin = strpbrk(begin, DELIMITER);
				begin++;
				len = strcspn(begin, DELIMITER);
				// isPlaying
				strncpy(isPlaying, begin, 5);
				LOGD("isPlaying = %s", isPlaying);

				jstring jartist = (*env)->NewStringUTF(env, artist);
				jstring jalbum = (*env)->NewStringUTF(env, album);
				jstring jtrack = (*env)->NewStringUTF(env, track);
				method = (*env)->GetMethodID(env, jc, "updateAudioMetadata", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");

				/* Update UI Activity with music metadata */
				if (method)
					(*env)->CallVoidMethod(env, jObj, method, jartist, jalbum, jtrack);
			}

			if (strncmp(buffer, PLAYSTATE_CHANGED, 23) == 0) {
				LOGD("PLAYSTATE_CHANGED");
				// could use isPlaying to show state change
			} else if (strncmp(buffer, META_CHANGED, 23) == 0) {
				LOGD("META_CHANGED");
				// could use data to show state change
			} else if (strncmp(buffer, QUEUE_CHANGED, 23) == 0) {
				LOGD("QUEUE_CHANGED");
				// could use data to show state change
			} else if (strncmp(buffer, PLAYBACK_COMPLETE, 23) == 0) {
				LOGD("PLAYBACK_COMPLETE");
				// could use data to show state change
			}
		}
		usleep(100);
	}
}

int chk(int err){
	switch(err){
	case 0:
		/* success */
		return 0;
	case LIBUSB_ERROR_IO:
		LOGE("LIBUSB_ERROR_IO: Input/output error.");
		break;
	case LIBUSB_ERROR_INVALID_PARAM:
		LOGE("LIBUSB_ERROR_INVALID_PARAM: Invalid parameter.");
		break;
	case LIBUSB_ERROR_ACCESS:
		LOGE("LIBUSB_ERROR_ACCESS: Access denied (insufficient permissions).");
		break;
	case LIBUSB_ERROR_NO_DEVICE:
		LOGE("LIBUSB_ERROR_NO_DEVICE: No such device (it may have been disconnected).");
		break;
	case LIBUSB_ERROR_NOT_FOUND:
		LOGE("LIBUSB_ERROR_NOT_FOUND: Entity not found.");
		break;
	case LIBUSB_ERROR_BUSY:
		LOGE("LIBUSB_ERROR_BUSY: Resource busy.");
		break;
	case LIBUSB_ERROR_TIMEOUT:
		LOGE("LIBUSB_ERROR_TIMEOUT: Operation timed out.");
		break;
	case LIBUSB_ERROR_OVERFLOW:
		LOGE("LIBUSB_ERROR_OVERFLOW: Overflow.");
		break;
	case LIBUSB_ERROR_PIPE:
		LOGE("LIBUSB_ERROR_PIPE: Pipe error.");
		break;
	case LIBUSB_ERROR_INTERRUPTED:
		LOGE("LIBUSB_ERROR_INTERRUPTED: System call interrupted (perhaps due to signal).");
		break;
	case LIBUSB_ERROR_NO_MEM:
		LOGE("LIBUSB_ERROR_NO_MEM: Insufficient memory.");
		break;
	case LIBUSB_ERROR_NOT_SUPPORTED:
		LOGE("LIBUSB_ERROR_NOT_SUPPORTED: Operation not supported or unimplemented on this platform.");
		break;
	case LIBUSB_ERROR_OTHER:
		LOGE("LIBUSB_ERROR_OTHER: Other error.");
		break;
	default:
		LOGE("Invalid LIBUSB error");
		break;
	}
	return err;
}
