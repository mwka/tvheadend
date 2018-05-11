#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <rtl-sdr.h>
#include <unistd.h>
#include <openssl/sha.h>

#include "tvheadend.h"
#include "input.h"
#include "rtlsdr_private.h"
#include "queue.h"
#include "fsmonitor.h"
#include "settings.h"

static htsmsg_t *
rtlsdr_adapter_class_save(idnode_t *in, char *filename, size_t fsize)
{
	rtlsdr_adapter_t *la = (rtlsdr_adapter_t*)in;
	htsmsg_t *m, *l;
	char ubuf[UUID_HEX_SIZE];

	m = htsmsg_create_map();
	idnode_save(&la->th_id, m);

	if (filename) {
		snprintf(filename, fsize, "input/rtlsdr/adapters/%s",
			idnode_uuid_as_str(&la->th_id, ubuf));
	}
	return m;
}

static void
rtlsdr_adapter_class_get_title
(idnode_t *in, const char *lang, char *dst, size_t dstsize)
{
	rtlsdr_adapter_t *la = (rtlsdr_adapter_t*)in;
	snprintf(dst, dstsize, "%s", la->device_name ? : la->product);
}

static const void *
rtlsdr_adapter_class_active_get(void *obj)
{
	static int active;
	rtlsdr_adapter_t *la = (rtlsdr_adapter_t*)obj;
	active = la->la_is_enabled(la);
	return &active;
}

static idnode_set_t *
rtlsdr_adapter_class_get_childs(idnode_t *in)
{
	rtlsdr_adapter_t *la = (rtlsdr_adapter_t*)in;
	idnode_set_t *is = idnode_set_create(0);
	return is;
}

const idclass_t rtlsdr_adapter_class =
{
	.ic_class = "rtlsdr_adapter",
	.ic_caption = N_("rtlsdr adapter"),
	.ic_event = "rtlsdr_adapter",
	.ic_save = rtlsdr_adapter_class_save,
	.ic_get_childs = rtlsdr_adapter_class_get_childs,
	.ic_get_title = rtlsdr_adapter_class_get_title,
	.ic_properties = (const property_t[]) {
		{
			.type = PT_BOOL,
				.id = "active",
				.name = N_("Active"),
				.opts = PO_RDONLY | PO_NOSAVE | PO_NOUI,
				.get = rtlsdr_adapter_class_active_get,
		},
	{
		.type = PT_INT,
		.id = "dev_index",
		.name = N_("Device index"),
		.desc = N_("Index of the device."),
		.opts = PO_RDONLY,
		.off = offsetof(rtlsdr_adapter_t, dev_index),
	},
			{}
}
};


/*
* Create
*/
static rtlsdr_adapter_t *
rtlsdr_adapter_create
(const char *uuid, htsmsg_t *conf,
	int number, char *vendor, char *product, char *serial, char *device_name)
{
	rtlsdr_adapter_t *la;

	/* Create */
	la = calloc(1, sizeof(rtlsdr_adapter_t));
	if (!tvh_hardware_create0((tvh_hardware_t*)la, &rtlsdr_adapter_class,
		uuid, conf)) {
		/* Note: la variable is freed in tvh_hardware_create0() */
		return NULL;
	}

	/* Setup */
	la->dev_index = number;
	la->vendor = strdup(vendor);
	la->product = strdup(product);
	la->serial = strdup(serial);
	la->device_name = strdup(device_name);
	/* Callbacks */
	la->la_is_enabled = rtlsdr_adapter_is_enabled;

	return la;
}

/*
*
*/
static rtlsdr_adapter_t *
rtlsdr_adapter_new(int number, char *vendor, char *product, char *serial, char *device_name, htsmsg_t **conf, int *save)
{
	rtlsdr_adapter_t *la;
	SHA_CTX sha1;
	uint8_t uuidbin[20];
	char uhex[UUID_HEX_SIZE];

	/* Create hash for adapter */
	SHA1_Init(&sha1);
	SHA1_Update(&sha1, number, sizeof(number));
	SHA1_Final(uuidbin, &sha1);

	bin2hex(uhex, sizeof(uhex), uuidbin, sizeof(uuidbin));

	/* Load config */
	*conf = hts_settings_load("input/rtlsdr/adapters/%s", uhex);
	if (*conf == NULL)
		*save = 1;

	/* Create */
	if (!(la = rtlsdr_adapter_create(uhex, *conf, number, vendor, product, serial, device_name))) {
		htsmsg_destroy(*conf);
		*conf = NULL;
		return NULL;
	}

	tvhinfo(LS_RTLSDR, "adapter added %d", number);
	return la;
}

static void
rtlsdr_adapter_add(int device_number)
{
	rtlsdr_adapter_t *la = NULL;
	htsmsg_t *conf = NULL;
	int save = 0;
	char vendor[256], product[256], serial[256];
	char *device_name;

	tvhtrace(LS_RTLSDR, "scanning adapter %d", device_number);
	rtlsdr_get_device_usb_strings(device_number, vendor, product, serial);
	device_name = rtlsdr_get_device_name(device_number);
	fprintf(stderr, "  %d:  %s, %s, SN: %s, %s\n", device_number, vendor, product, serial, device_name);
	la = rtlsdr_adapter_new(device_number, vendor, product, serial, device_name, &conf, &save);
	if (la == NULL) {
		tvherror(LS_RTLSDR, "failed to create %d", device_number);
		return; // Note: save to return here as global_lock is held
	}
	if (conf)
		htsmsg_destroy(conf);
	/* Save configuration */
	if (save && la)
		rtlsdr_adapter_changed(la);
}

void rtlsdr_init() {
	int i, device_count;
	idclass_register(&rtlsdr_adapter_class);
	idclass_register(&rtlsdr_frontend_dab_class);
	/*---------------------------------------------------
	Looking for device and open connection
	----------------------------------------------------*/
	device_count = rtlsdr_get_device_count();
	if (!device_count) {
		fprintf(stderr, "No supported devices found.\n");
		exit(1);
	}

	fprintf(stderr, "Found %d device(s):\n", device_count);
	for (i = 0; i < device_count; i++) {
		rtlsdr_adapter_add(i);
	}
	fprintf(stderr, "\n");
}

