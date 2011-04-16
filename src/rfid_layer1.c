#include <ccid.h>
#include <unistd.h>

#include "ccid-internal.h"
#include "rfid-internal.h"

int _rfid_init(struct _cci *cci,
			const struct rfid_layer1_ops *ops,
			void *priv)
{
	struct _rfid *rf;

	if ( !(*ops->rf_power)(cci->cc_parent, priv, 0) )
		return 0;

	rf = calloc(1, sizeof(*rf));
	if ( NULL == rf )
		return 0;

	rf->rf_ccid = cci->cc_parent;
	rf->rf_l1 = ops;
	rf->rf_l1p = priv;

	cci->cc_priv = rf;

	return 1;
}

int _rfid_layer1_rf_power(struct _cci *cci, unsigned int on)
{
	struct _rfid *rf = cci->cc_priv;
	return (*rf->rf_l1->rf_power)(cci->cc_parent, rf->rf_l1p, on);
}

int _rfid_layer1_14443a_init(struct _cci *cci)
{
	struct _rfid *rf = cci->cc_priv;
	return (*rf->rf_l1->iso14443a_init)(cci->cc_parent, rf->rf_l1p);
}

int _rfid_layer1_set_rf_mode(struct _cci *cci, const struct rf_mode *mode)
{
	struct _rfid *rf = cci->cc_priv;
	return (*rf->rf_l1->set_rf_mode)(cci->cc_parent, rf->rf_l1p, mode);
}

int _rfid_layer1_get_rf_mode(struct _cci *cci, const struct rf_mode *mode)
{
	struct _rfid *rf = cci->cc_priv;
	return (*rf->rf_l1->get_rf_mode)(cci->cc_parent, rf->rf_l1p, mode);
}

int _rfid_layer1_get_error(struct _cci *cci, uint8_t *err)
{
	struct _rfid *rf = cci->cc_priv;
	return (*rf->rf_l1->get_error)(cci->cc_parent, rf->rf_l1p, err);
}

int _rfid_layer1_get_coll_pos(struct _cci *cci, uint8_t *pos)
{
	struct _rfid *rf = cci->cc_priv;
	return (*rf->rf_l1->get_coll_pos)(cci->cc_parent, rf->rf_l1p, pos);
}

int _rfid_layer1_set_speed(struct _cci *cci, unsigned int i)
{
	struct _rfid *rf = cci->cc_priv;
	return (*rf->rf_l1->set_speed)(cci->cc_parent, rf->rf_l1p, i);
}

int _rfid_layer1_transact(struct _cci *cci,
					const uint8_t *tx_buf,
					uint8_t tx_len,
					uint8_t *rx_buf,
					uint8_t *rx_len,
					uint64_t timer,
					unsigned int toggle)
{
	struct _rfid *rf = cci->cc_priv;
	return (*rf->rf_l1->transact)(cci->cc_parent, rf->rf_l1p,
					 tx_buf, tx_len,
					 rx_buf, rx_len,
					 timer, toggle);
}

unsigned int _rfid_layer1_carrier_freq(struct _cci *cci)
{
	struct _rfid *rf = cci->cc_priv;
	return (*rf->rf_l1->carrier_freq)(cci->cc_parent, rf->rf_l1p);
}

unsigned int _rfid_layer1_get_speeds(struct _cci *cci)
{
	struct _rfid *rf = cci->cc_priv;
	return (*rf->rf_l1->get_speeds)(cci->cc_parent, rf->rf_l1p);
}

unsigned int _rfid_layer1_mtu(struct _cci *cci)
{
	struct _rfid *rf = cci->cc_priv;
	return (*rf->rf_l1->mtu)(cci->cc_parent, rf->rf_l1p);
}

unsigned int _rfid_layer1_mru(struct _cci *cci)
{
	struct _rfid *rf = cci->cc_priv;
	return (*rf->rf_l1->mru)(cci->cc_parent, rf->rf_l1p);
}
