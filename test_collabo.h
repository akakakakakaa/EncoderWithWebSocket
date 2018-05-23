#include <stdio.h>
#include <Collabo.hpp>
#include "qemu/thread.h"	// for thread
#include "ui/console.h"		// for capture
#include "ui/keymaps.h"		// for SCANCODE
#include <signal.h>
#include "collabo-keymaps.h"

QemuThread* v_thread;
bool stopped = false;
//for video
int width = 1920;
int height = 1080;

//for audio
int samplerate = 48000;
int channels = 1;
CaptureVoiceOut* audio_cap;
struct ClbContext clbCtx;

//for mouse
int last_x = 0;
int last_y = 0;

//for socket
int collabo_port;
int agent_port;
const char* meet_seq;
const char* password;

void collabo_run(int m_port, int m_agent_port, const char* m_meet_seq, const char* m_password);
void signal_event(int s);
void* collabo_thread_func(void* ptr);
void qemu_video_init(void);
uint8_t* request_video_callback(int* width, int* height);
void qemu_video_stop(void);
void mouse_callback(int x, int y, int dw_flag);
void kbd_callback(int kbd_code, bool kbd_flag);
void state_callback(enum ClbState state);
static void audio_capture_notify(void* opaque, audcnotification_e cmd);
static void audio_capture_destroy(void* opaque);
static void audio_capture(void* opaque, void* buf, int size);

void collabo_run(int m_port, int m_agent_port, const char* m_meet_seq, const char* m_password) {
	collabo_port = m_port;
	agent_port = m_agent_port;
	meet_seq = m_meet_seq;
	password = m_password;

	printf("collabo start. port is %d\n", collabo_port);
	QemuThread* run_thread = g_malloc0(sizeof(QemuThread));
	qemu_thread_create(run_thread, "run", collabo_thread_func, NULL, QEMU_THREAD_DETACHED);
}

void signal_event(int s) {
	printf("clbStop\n");
	qemu_video_stop();
	clbStop();
	exit(0);
}
	
void* collabo_thread_func(void* ptr) {
        //initialize video options
	sleep(5);
	QemuConsole* con;
	
	int i=0;
	for(; ;i++) {
        	con = qemu_console_lookup_by_index(i);
		if(con == NULL)
			printf("cannot find QemuConsole %d\n", i);
		else {
			printf("console index is %d\n", i);
			break;
		}
	}
        graphic_hw_update(con);

        DisplaySurface* surface = qemu_console_surface(con);
	if(surface == NULL) {
		printf("DisplaySurface is NULL\n");
		exit(1);
	}	

        width = pixman_image_get_width(surface->image);
        height = pixman_image_get_height(surface->image);

	printf("setting complete\n");
	//receive ctrl+c
	struct sigaction handler;
	handler.sa_handler = signal_event;
	sigemptyset(&handler.sa_mask);
	handler.sa_flags = 0;
	sigaction(SIGINT, &handler, NULL);

	//init audio capture
        struct audsettings aset;
        aset.freq = samplerate;
        aset.nchannels = channels;
        aset.fmt = AUD_FMT_S16;
        aset.endianness = AUDIO_HOST_ENDIANNESS;

        struct audio_capture_ops ops;
        ops.notify = audio_capture_notify;
        ops.destroy = audio_capture_destroy;
        ops.capture = audio_capture;

	//set clbctx audio info
	clbCtx.isAudioEnabled = true;
	clbCtx.samplerate = samplerate;
	clbCtx.channels = channels;
	clbCtx.inSmpFmt = SMP_FMT_S16;
	clbCtx.isSilent = true;

	//init audio capture        
	audio_cap = AUD_add_capture(&aset, &ops, NULL);
        if(!audio_cap)
                printf("audio init fail\n");

	//start webserver and encoder
	clbStart(collabo_port, agent_port, meet_seq, password, mouse_callback, kbd_callback, state_callback);	

	return NULL;
}

pixman_image_t* linebuf = NULL;
void qemu_video_init(void) {
	QemuConsole* con = qemu_console_lookup_by_index(0);
	graphic_hw_update(con);

	DisplaySurface* surface = qemu_console_surface(con);

	int width = pixman_image_get_width(surface->image);
	int height = pixman_image_get_height(surface->image);

	if(linebuf != NULL)
		qemu_pixman_image_unref(linebuf);
	linebuf = qemu_pixman_linebuf_create(PIXMAN_BE_r8g8b8, width);

	clbCtx.isVideoEnabled = true;
	clbCtx.width = width;
	clbCtx.height = height;
	clbCtx.inPixFmt = PIX_FMT_R8G8B8;	

	startStreaming(clbCtx, &request_video_callback);
	printf("qemu_video_init\n");
	
	stopped = false;
}

uint8_t* request_video_callback(int* e_width, int* e_height) {
	QemuConsole* con = qemu_console_lookup_by_index(0);
	graphic_hw_update(con);

	DisplaySurface* surface = qemu_console_surface(con);
		
	*e_width = pixman_image_get_width(surface->image);
	*e_height = pixman_image_get_height(surface->image);
	if(*e_width != width || *e_height != height) {
		width = *e_width;
		height = *e_height;	

		qemu_pixman_image_unref(linebuf);
		linebuf = qemu_pixman_linebuf_create(PIXMAN_BE_r8g8b8, width);
	}

	uint8_t* cur_frame = malloc((*e_width)*(*e_height)*3);
	int offset = 0;
	int y = 0;
	for(; y<height; y++) {
		qemu_pixman_linebuf_fill(linebuf, surface->image, width, 0, y);
		memcpy(cur_frame+offset, pixman_image_get_data(linebuf), width*3);
		offset += width*3;
	}

	return cur_frame;
}

void qemu_video_stop(void) {
	stopped = true;
	//qemu_pixman_image_unref(linebuf);
	stopStreaming();
	printf("qemu_video_release\n");
}

struct timeval start2, end2;
int size2=0;
bool audioEnabled = false;
static void audio_capture_notify(void* opaque, audcnotification_e cmd) {
	switch(cmd) {
	case AUD_CNOTIFY_ENABLE:
		printf("audio enabled\n");
		setSilent(false);		
		clbCtx.isSilent = false;
		break;
	case AUD_CNOTIFY_DISABLE:
		printf("audio disabled\n");
		setSilent(true);
		clbCtx.isSilent = true;
		break;
	}
}

static void audio_capture_destroy(void* opaque) {
}

static void audio_capture(void* opaque, void* buf, int size) {
	if(!stopped && clbCtx.isAudioEnabled)
		encodeAudio(buf, size);
}

void mouse_callback(int x, int y, int button_mask) {
	static uint32_t last_bmask;
	static uint32_t bmap[INPUT_BUTTON__MAX] = {
		[INPUT_BUTTON_LEFT]       = 0x01,
		[INPUT_BUTTON_MIDDLE]     = 0x02,
		[INPUT_BUTTON_RIGHT]      = 0x04,
		[INPUT_BUTTON_WHEEL_UP]   = 0X08,
		[INPUT_BUTTON_WHEEL_DOWN] = 0x10,
	};

	QemuConsole* con = qemu_console_lookup_by_index(0);
	DisplaySurface* surface = qemu_console_surface(con);

	int width = pixman_image_get_width(surface->image);
	int height = pixman_image_get_height(surface->image);

	if(last_bmask != button_mask) {
		qemu_input_update_buttons(con, bmap, last_bmask, button_mask);
		last_bmask = button_mask;
	}
	
	if(qemu_input_is_absolute()) {
		/*
		printf("aa\n");
		last_x += x;
		last_y += y;
		if(last_x < 0)
			last_x = 0;
		else if(last_x >= width)
			last_x = width - 1;
		
		if(last_y < 0)
			last_y = 0;
		else if(last_y >= height)
			last_y = height - 1;		
		*/
		printf("%d %d\n", x, y);
		qemu_input_queue_abs(con, INPUT_AXIS_X, x, 0, width);
		qemu_input_queue_abs(con, INPUT_AXIS_Y, y, 0, height);
	}
	else {
		qemu_input_queue_rel(con, INPUT_AXIS_X, x);
		qemu_input_queue_rel(con, INPUT_AXIS_Y, y);
	}

	qemu_input_event_sync();
}

void kbd_callback(int kbd_code, bool kbd_flag) {
	/*
	if(qemu_console_is_graphic(NULL)) {
		KeyValue keyvalue;
		uint32_t number;

		keyvalue.type = KEY_VALUE_KIND_QCODE;
		keyvalue.u.qcode.data = qemu_input_map_win32_to_qcode[kbd_code];
		number = qemu_input_key_value_to_number(&keyvalue);
		qemu_input_event_send_key_number(NULL, number, kbd_flag);
	}
	*/
	int key_code = keycode_from_ascii(kbd_code);	

	if(qemu_console_is_graphic(NULL))
		qemu_input_event_send_key_number(NULL, key_code, kbd_flag);
}

void state_callback(enum ClbState state) {
	switch(state) {
		case START_STREAMING: {
			printf("START_STREAMING called\n");
			qemu_video_init();
			break;
		}
		case STOP_STREAMING: {
			printf("STOP_STREAMING called\n");
			qemu_video_stop();
			break;
		}
		case STOP_PROGRAM: {
			printf("STOP_PROGRAM called\n");
			qemu_video_stop();
			clbStop();
			exit(0);
			break;
		}
	}
}
