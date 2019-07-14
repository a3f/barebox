/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 Ahmad Fatoum, Pengutronix
 */

#ifndef __DWMAC4_H_
#define __DWMAC4_H_

int eqos_get_ethaddr(struct eth_device *edev, unsigned char *mac);
int eqos_set_ethaddr(struct eth_device *edev, const unsigned char *mac);
int eqos_start(struct eth_device *edev);
void eqos_stop(struct eth_device *edev);
int eqos_send(struct eth_device *edev, void *packet, int length);
int eqos_recv(struct eth_device *edev);
int eqos_init(struct dw_eth_dev *eqos);
void eqos_adjust_link(struct eth_device *edev);

void eqos_remove_resources(struct device_d *dev); // FIXME removable?

#endif
