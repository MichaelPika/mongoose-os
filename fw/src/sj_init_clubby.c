/*
 * Copyright (c) 2014-2016 Cesanta Software Limited
 * All rights reserved
 */

#include "common/clubby/clubby_channel_ws.h"

#ifdef SJ_ENABLE_CLUBBY

#include "fw/src/clubby_channel_uart.h"
#include "fw/src/mg_uart.h"
#include "fw/src/sj_config.h"
#include "fw/src/sj_init_clubby.h"
#include "fw/src/sj_mongoose.h"
#include "fw/src/sj_sys_config.h"
#include "fw/src/sj_wifi.h"

#undef connect /* CC3200 redefines it to sl_Connect */

static struct clubby *s_global_clubby;

static void clubby_wifi_ready(enum sj_wifi_status event, void *arg) {
  if (event != SJ_WIFI_IP_ACQUIRED) return;
  struct clubby_channel *ch = (struct clubby_channel *) arg;
  ch->connect(ch);
}

struct clubby_cfg *clubby_cfg_from_sys(const struct sys_config_clubby *sccfg) {
  struct clubby_cfg *ccfg = (struct clubby_cfg *) calloc(1, sizeof(*ccfg));
  sj_conf_set_str(&ccfg->id, sccfg->device_id);
  sj_conf_set_str(&ccfg->psk, sccfg->device_psk);
  ccfg->max_queue_size = sccfg->max_queue_size;
  return ccfg;
}

enum sj_init_result clubby_init(void) {
  const struct sys_config_clubby *sccfg = &get_cfg()->clubby;
  if (sccfg->device_id != NULL) {
    struct clubby_cfg *ccfg = clubby_cfg_from_sys(sccfg);
    struct clubby *c = clubby_create(ccfg);
    if (sccfg->server_address != NULL) {
      struct clubby_channel_ws_out_cfg *chcfg =
          clubby_channel_ws_out_cfg_from_sys(sccfg);
      struct clubby_channel *ch = clubby_channel_ws_out(&sj_mgr, chcfg);
      if (ch == NULL) {
        return SJ_INIT_CLUBBY_FAILED;
      }
      clubby_add_channel(c, mg_mk_str(MG_CLUBBY_DST_DEFAULT), ch,
                         false /* is_trusted */, true /* send_hello */);
      if (sccfg->connect_on_boot) {
        if (get_cfg()->wifi.sta.enable) {
          sj_wifi_add_on_change_cb(clubby_wifi_ready, ch);
        } else {
          clubby_connect(c);
        }
      }
    }
    if (sccfg->uart.uart_no >= 0) {
      const struct sys_config_clubby_uart *scucfg = &get_cfg()->clubby.uart;
      struct mg_uart_config *ucfg = mg_uart_default_config();
      ucfg->baud_rate = scucfg->baud_rate;
      ucfg->rx_fc_ena = ucfg->tx_fc_ena = scucfg->fc_enable;
      if (mg_uart_init(scucfg->uart_no, ucfg, NULL, NULL) != NULL) {
        struct clubby_channel *uch = clubby_channel_uart(scucfg->uart_no);
        clubby_add_channel(c, mg_mk_str(""), uch, true /* is_trusted */,
                           false /* send_hello */);
        if (sccfg->connect_on_boot) uch->connect(uch);
      }
    }
    s_global_clubby = c;
  }
  return SJ_INIT_OK;
}

struct clubby_channel_ws_out_cfg *clubby_channel_ws_out_cfg_from_sys(
    const struct sys_config_clubby *sccfg) {
  struct clubby_channel_ws_out_cfg *chcfg =
      (struct clubby_channel_ws_out_cfg *) calloc(1, sizeof(*chcfg));
  sj_conf_set_str(&chcfg->server_address, sccfg->server_address);
  sj_conf_set_str(&chcfg->ssl_ca_file, sccfg->ssl_ca_file);
  sj_conf_set_str(&chcfg->ssl_client_cert_file, sccfg->ssl_client_cert_file);
  sj_conf_set_str(&chcfg->ssl_server_name, sccfg->ssl_server_name);
  chcfg->reconnect_interval_min = sccfg->reconnect_timeout_min;
  chcfg->reconnect_interval_max = sccfg->reconnect_timeout_max;
  return chcfg;
}

struct clubby *clubby_get_global(void) {
  return s_global_clubby;
}
#endif /* SJ_ENABLE_CLUBBY */