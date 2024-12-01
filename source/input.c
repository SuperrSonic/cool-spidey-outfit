#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <stdio.h>
#include <wiiuse/wpad.h>

#include "input.h"

typedef struct input_driver
{
	struct input_driver* (*init)(struct input_driver*);
	void (*shutdown)(struct input_driver* this);
	int (*poll)(struct input_driver* this, unsigned int *pressed);
} input_driver;

static input_driver *g_in;
#ifdef HW_RVL
static const input_driver psx_interface;
static const input_driver wpad_interface;
#endif
static const input_driver gc_interface;
#ifdef HW_RVL
/***** BEGIN PSX-USB *****/

#define PSX_VID     0x0810
#define PSX_PID     0x0003
#define PSX_EP      0x81

typedef struct psx_driver
{
	input_driver driver;
	input_driver *next;
	s32 fd;
	u8 data[8];
	u8 *usb_data;
	u8 new_data;
	volatile u8 polling;
	u8 new_device;
} psx_driver;

static s32 psx_cb(s32 result, void *cb_data)
{
	psx_driver *psx = (psx_driver*)cb_data;

	if (psx==NULL)
		return -1;
	psx->polling = 0;

	if (result < 0 || psx->usb_data==NULL || psx->fd==0)
		return -1;

	if (result==8)
	{
		memcpy(psx->data, psx->usb_data, 8);
		psx->new_data = 1;
	}

	if (USB_ReadIntrMsgAsync(psx->fd, PSX_EP, 8, psx->usb_data, psx_cb, psx)>=0)
		psx->polling = 1;

	return 0;
}

static s32 psx_devicechange(s32 result, void *cb_data)
{
	psx_driver *psx = (psx_driver*)cb_data;

	if (psx==NULL || result<0)
		return -1;

	psx->new_device = 1;

	// FIXME: no libogc function to release device change hook
	if (psx->usb_data)
		return USB_DeviceChangeNotifyAsync(USB_CLASS_HID, psx_devicechange, cb_data);

	return 0;
}

static void psx_shutdown(input_driver* driver)
{
	psx_driver *psx = (psx_driver*)driver;

	if (psx==NULL)
		return;

	if (psx->usb_data)
	{
		void *p = psx->usb_data;
		psx->usb_data = NULL;
		while (psx->polling);
		iosFree(0, p);
	}

	if (psx->fd!=0)
	{
		USB_CloseDevice(&psx->fd);
		psx->fd = 0;
	}

	if (psx->next)
		psx->next->shutdown(psx->next);

	free(psx);
}

static const unsigned int psx_hat_to_dpad[16] =
{
	INPUT_BUTTON_UP,
	INPUT_BUTTON_UP | INPUT_BUTTON_RIGHT,
	INPUT_BUTTON_RIGHT,
	INPUT_BUTTON_RIGHT | INPUT_BUTTON_DOWN,
	INPUT_BUTTON_DOWN,
	INPUT_BUTTON_DOWN | INPUT_BUTTON_LEFT,
	INPUT_BUTTON_LEFT,
	INPUT_BUTTON_LEFT | INPUT_BUTTON_UP,
};

static int psx_poll(input_driver *driver, unsigned int *pressed)
{
	psx_driver *psx = (psx_driver*)driver;
	int read = 0;

	if (psx==NULL)
		return 0;

	if (psx->new_device)
	{
		u8 i, count;
		usb_device_entry devices[32];

		psx->new_device = 0;

		if (USB_GetDeviceList(devices, sizeof(devices)/sizeof(devices[0]), USB_CLASS_HID, &count)>=0)
		{
			if (psx->fd) // check if device was unplugged
			{
				u8 found = 0;
				for (i=0; i < count; i++)
				{
					// device_id==0 indicates old IOS being used, check VID/PID only
					if (devices[i].device_id==psx->fd || (devices[i].device_id==0 && devices[i].vid==PSX_VID && devices[i].pid==PSX_PID))
					{
						found = 1;
						break;
					}
				}

				// device was unplugged
				if (!found)
				{
					psx->fd = 0;
					while (psx->polling);
					psx->new_data = 0;
				}
			}
			else // look for new psx device
			{
				for (i=0; i < count; i++)
				{
					if (devices[i].vid==PSX_VID && devices[i].pid==PSX_PID && USB_OpenDevice(devices[i].device_id, PSX_VID, PSX_PID, &psx->fd)>=0)
					{
						psx_cb(0, psx); // start polling
						break;
					}
				}
			}
		}

	}

	if (psx->new_data)
	{
		*pressed |= psx_hat_to_dpad[psx->data[5]&0xF];
		if (psx->data[6] & 0x20) // start
			*pressed |= INPUT_BUTTON_START;
		if (psx->data[5] & 0x80) // square
			*pressed |= INPUT_BUTTON_CANCEL;
		if (psx->data[5] & 0x40) // cross
			*pressed |= INPUT_BUTTON_OK;
		if (psx->data[5] & 0x20) // circle
			*pressed |= INPUT_BUTTON_2;
		if (psx->data[5] & 0x10) // triangle
			*pressed |= INPUT_BUTTON_1;

		psx->new_data = 0;
		read = 1;
	}

	if (psx->next)
		read += psx->next->poll(psx->next, pressed);

	return read;
}

static input_driver* psx_init(input_driver* in)
{
	psx_driver *psx = (psx_driver*)malloc(sizeof(psx_driver));
	if (psx==NULL)
		return in;

	psx->usb_data = iosAlloc(0, 8);
	if (psx->usb_data==NULL)
	{
		free(psx);
		return in;
	}

	psx->driver = psx_interface;
	psx->next = in;
	psx->fd = 0;
	psx->new_data = 0;
	psx->new_device = 1;
	psx->polling = 0;

	// register for device change
	if (psx_devicechange(0, psx)<0)
	{
		iosFree(0, psx->usb_data);
		free(psx);
		return in;
	}

	return &psx->driver;
}

/***** END PSX *****/

/***** BEGIN WIIMOTE *****/
#if 1
typedef struct wpad_driver
{
	input_driver driver;
	input_driver *next;
	struct wiimote_t** wiimotes;
	int wpad_initted;
	int wpad_connected;
	uint8_t wm_data[WPAD_MAX_WIIMOTES][64];
} wpad_driver;

static wpad_driver *_wpad;

static void wpad_shutdown(input_driver *driver)
{
	wpad_driver *wpad = (wpad_driver*)driver;

	if (wpad==NULL)
		return;

	if (wpad->wpad_initted)
	{
		WPAD_Shutdown();
		wpad->wpad_initted = 0;
	}

	if (wpad->next)
		wpad->next->shutdown(wpad->next);

	wpad->next = NULL;
}

static int wpad_poll(input_driver *driver, unsigned int *pressed)
{
	wpad_driver *wpad = (wpad_driver*)driver;
	int read = 0;
	const WPADData *wd;

	if (wpad && wpad->wpad_initted)
	{
		int i;

		// This stuff isn't used yet
		for (i=0; i < WPAD_MAX_WIIMOTES; i++)
		{
			if (wpad->wpad_connected & (1<<i) && WPAD_Probe(i, NULL)==WPAD_ERR_NO_CONTROLLER)
			{
//				printf("Wiimote %d disconnected\n", i);
				wpad->wpad_connected &= ~(1<<i);
			}
			else if ((wpad->wpad_connected & (1<<i))==0 && WPAD_Probe(i, NULL) == WPAD_ERR_NONE)
			{
//				printf("Wiimote %d connected (%p)\n", i, wpad->wm_data[i]);
				wpad->wpad_connected |= (1<<i);
//				wiiuse_read_data(wpad->wiimotes[i], wpad->wm_data[i], , sizeof(wpad->wm_data[0]), wm_read_cb);
			}
		}

		if (WPAD_ReadPending(WPAD_CHAN_0, NULL)>0 && (wd = WPAD_Data(WPAD_CHAN_0)) && (wd->data_present&WPAD_DATA_BUTTONS))
		{
			if (wd->btns_d & WPAD_BUTTON_DOWN)
				*pressed |= INPUT_BUTTON_DOWN;
			if (wd->btns_d & WPAD_BUTTON_UP)
				*pressed |= INPUT_BUTTON_UP;
			if (wd->btns_d & WPAD_BUTTON_LEFT)
				*pressed |= INPUT_BUTTON_LEFT;
			if (wd->btns_d & WPAD_BUTTON_RIGHT)
				*pressed |= INPUT_BUTTON_RIGHT;
			if (wd->btns_d & WPAD_BUTTON_A)
				*pressed |= INPUT_BUTTON_OK;
			if (wd->btns_d & WPAD_BUTTON_B)
				*pressed |= INPUT_BUTTON_CANCEL;
			if (wd->btns_d & WPAD_BUTTON_1)
				*pressed |= INPUT_BUTTON_1;
			if (wd->btns_d & WPAD_BUTTON_2)
				*pressed |= INPUT_BUTTON_2;
			if (wd->btns_d & WPAD_BUTTON_HOME)
				*pressed |= INPUT_BUTTON_START;

			if (wd->exp.type == WPAD_EXP_CLASSIC)
			{
				if (wd->btns_d & WPAD_CLASSIC_BUTTON_DOWN)
					*pressed |= INPUT_BUTTON_DOWN;
				if (wd->btns_d & WPAD_CLASSIC_BUTTON_UP)
					*pressed |= INPUT_BUTTON_UP;
				if (wd->btns_d & WPAD_CLASSIC_BUTTON_LEFT)
					*pressed |= INPUT_BUTTON_LEFT;
				if (wd->btns_d & WPAD_CLASSIC_BUTTON_RIGHT)
					*pressed |= INPUT_BUTTON_RIGHT;
				if (wd->btns_d & WPAD_CLASSIC_BUTTON_B)
					*pressed |= INPUT_BUTTON_OK;
				if (wd->btns_d & WPAD_CLASSIC_BUTTON_Y)
					*pressed |= INPUT_BUTTON_CANCEL;
				if (wd->btns_d & WPAD_CLASSIC_BUTTON_X)
					*pressed |= INPUT_BUTTON_1;
				if (wd->btns_d & WPAD_CLASSIC_BUTTON_A)
					*pressed |= INPUT_BUTTON_2;
				if (wd->btns_d & WPAD_CLASSIC_BUTTON_HOME)
					*pressed |= INPUT_BUTTON_START;
			}

			read = 1;
		}
	}

	if (wpad->next)
		read += wpad->next->poll(wpad->next, pressed);

	return read;
}

static input_driver* wpad_init(input_driver *in)
{
	wpad_driver *wpad = _wpad;

	if (wpad && wpad->wpad_initted)
		return in;

	if (wpad == NULL)
	{
		wpad = (wpad_driver*)malloc(sizeof(wpad_driver));
		if (wpad==NULL)
			return in;
		_wpad = wpad;
		// get wiimote_t array before WPAD_Init() calls wiiuse_init()
		wpad->wiimotes = wiiuse_init(WPAD_MAX_WIIMOTES, NULL);
		wpad->wpad_connected = 0;
	}
	else if (wpad->wpad_initted)
		return in;


	WPAD_Init();
	wpad->wpad_initted = 1;

	wpad->driver = wpad_interface;
	wpad->next = in;

	return &wpad->driver;
}
#endif
#endif
/***** END WIIMOTE *****/

/***** BEGIN GAMECUBE *****/

typedef struct gc_driver
{
	input_driver driver;
	input_driver *next;
} gc_driver;

static int gc_initted;

static int gc_poll(input_driver *driver, unsigned int *pressed)
{
	gc_driver *gc = (gc_driver*)driver;
	int read = 0;

	if (PAD_ScanPads() & 1) // check for controller 0 being connected
	{
		u16 buttons = PAD_ButtonsHeld(PAD_CHAN0);

		if (buttons & PAD_BUTTON_DOWN)
			*pressed |= INPUT_BUTTON_DOWN;
		if (buttons & PAD_BUTTON_UP)
			*pressed |= INPUT_BUTTON_UP;
		if (buttons & PAD_BUTTON_LEFT)
			*pressed |= INPUT_BUTTON_LEFT;
		if (buttons & PAD_BUTTON_RIGHT)
			*pressed |= INPUT_BUTTON_RIGHT;
		if (buttons & PAD_BUTTON_A)
			*pressed |= INPUT_BUTTON_OK;
		if (buttons & PAD_BUTTON_B)
			*pressed |= INPUT_BUTTON_CANCEL;
		if (buttons & PAD_BUTTON_Y)
			*pressed |= INPUT_BUTTON_1;
		if (buttons & PAD_BUTTON_X)
			*pressed |= INPUT_BUTTON_2;
		if (buttons & PAD_BUTTON_START)
			*pressed |= INPUT_BUTTON_START;

		read = 1;
	}

	if (gc->next)
		read += gc->next->poll(gc->next, pressed);

	return read;
}

static void gc_shutdown(input_driver *driver)
{
	gc_driver *gc = (gc_driver*)driver;

	// nothing to do here...

	if (gc->next)
		gc->next->shutdown(gc->next);

	free(gc);
	gc_initted = 0;
}

static input_driver* gc_init(input_driver *in)
{
	gc_driver *gc;

	if (gc_initted || PAD_Init() < 0)
		return in;

	gc = (gc_driver*)malloc(sizeof(gc_driver));
	if (gc==NULL)
		return in;

	gc->driver = gc_interface;
	gc->next = in;
	gc_initted = 1;

	return &gc->driver;
}


/***** END GAMECUBE *****/

#ifdef HW_RVL
static const input_driver psx_interface = {psx_init, psx_shutdown, psx_poll};
static const input_driver wpad_interface = {wpad_init, wpad_shutdown, wpad_poll};
#endif
static const input_driver gc_interface = {gc_init, gc_shutdown, gc_poll};

// put psx after wiimote so USB system has time to settle
static const input_driver* g_drivers[] =
{
#ifdef HW_RVL
	&wpad_interface,
#endif
	&gc_interface,
//	&psx_interface,
	NULL
};


void input_startup(void)
{
	unsigned int i;

	for (i=0; g_drivers[i]; i++)
		g_in = g_drivers[i]->init(g_in);
}

void input_shutdown(void)
{
	if (g_in)
	{
		g_in->shutdown(g_in);
		g_in = NULL;
	}
}

/* fetches current button state, returns number of devices successfully read */
unsigned int get_input(unsigned int *pressed, unsigned int *down)
{
	static unsigned int last;
	unsigned int buttons = 0;
	unsigned int read = 0;

	if (g_in)
		read = g_in->poll(g_in, &buttons);

	if (pressed)
		*pressed = buttons;
	if (down)
		*down = ~last & buttons;
	last = buttons;
	return read;
}