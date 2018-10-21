#include "mgos.h"
#include "mgos_mqtt.h"
#ifdef MGOS_HAVE_WIFI
#include "mgos_wifi.h"
#endif
#include "mgos_shadow.h"
#include "mgos_crontab.h"

#define LED1_PIN mgos_sys_config_get_board_led1_pin()
#define RELAY1_PIN 14
#define RELAY2_PIN 12
#define BUTTON1_PIN 5
#define FMT "{led: %B, relay1: %B, relay2: %B}"

static void report_state(void) {
    mgos_shadow_updatef(0, FMT,
                        mgos_gpio_read(LED1_PIN),
                        mgos_gpio_read(RELAY1_PIN),
                        mgos_gpio_read(RELAY2_PIN));
}

static void shadow_cb(int ev, void *evd, void *arg) {
    LOG(LL_INFO, ("Shawdow Event: %d, version: 0", ev));
    if (ev == MGOS_SHADOW_CONNECTED)
    {
        LOG(LL_INFO, ("Shawdow: Reporting connected state"));
        report_state();
        return;
    }

    if (ev != MGOS_SHADOW_GET_ACCEPTED &&
        ev != MGOS_SHADOW_UPDATE_DELTA)
    {
        LOG(LL_INFO, ("Shawdow: Get/Update successful"));
        return;
    }

    struct mg_str * event_data = (struct mg_str *)evd;

    LOG(LL_INFO, ("Shawdow Response: %.*s", (int) event_data->len, event_data->p));

    /* Synchronise state and set actuals if changed */
    //json_scanf(event_data->p, event_data->len, FMT, &s_led_reading, &s_relay1_reading, &s_relay2_reading);

    if (ev == MGOS_SHADOW_UPDATE_DELTA)
    {
        LOG(LL_INFO, ("Shawdow: Synchronise state locally"));
        //mgos_gpio_write(LED1_PIN, s_led_reading);
        //mgos_gpio_write(RELAY1_PIN, s_relay1_reading);
        //mgos_gpio_write(RELAY2_PIN, s_relay2_reading);

        LOG(LL_INFO, ("Shawdow: Reporting updated state"));
        report_state();
    }

    (void) arg;
}

void pump_action(char action[5]) {
  bool action_bool = 1;
  if (strcmp(action, "toggle") == 0)
  {
    action_bool = mgos_gpio_toggle(RELAY1_PIN);
  }
  else
  {
    if (strcmp(action, "on") == 0)
    {
      action_bool = 0;
    }
    else if (strcmp(action, "off") == 0)
    {
      action_bool = 1;
    }
    mgos_gpio_write(RELAY1_PIN, action_bool);
  }
  //s_relay1_reading = action_bool;
  char topic[100], message[160];
  struct json_out out = JSON_OUT_BUF(message, sizeof(message));

  time_t now=time(0);
  struct tm *timeinfo = localtime(&now);

  snprintf(topic, sizeof(topic), "pump/status");
  json_printf(&out, "{pump: \"%s\", free_ram: %lu, device: \"%s\", timestamp: \"%02d:%02d:%02d\"}",
              (char *) action,
              (unsigned long) mgos_get_free_heap_size(),
              (char *) mgos_sys_config_get_device_id(),
              (int) timeinfo->tm_hour,
              (int) timeinfo->tm_min,
              (int) timeinfo->tm_sec);

  bool res = mgos_mqtt_pub(topic, message, strlen(message), 1, false);
  LOG(LL_INFO, ("Published to MQTT: %s", res ? "yes" : "no"));
  report_state();
  LOG(LL_INFO, ("Relay1 set -> %s", action));
}

void pump_cb(struct mg_str action, struct mg_str payload, void *userdata) {
  char topic[100], message[160], action_passed[3];
  struct json_out out = JSON_OUT_BUF(message, sizeof(message));

  time_t t = time(0);
  struct tm* timeinfo = localtime(&t);

  snprintf(topic, sizeof(topic), "crontab");
  json_printf(&out, "{set_pump: \"%s\", free_ram: %lu, device: \"%s\", timestamp: \"%02d:%02d:%02d\"}",
              (char *) action.p,
              (unsigned long) mgos_get_free_heap_size(),
              (char *) mgos_sys_config_get_device_id(),
              (int) timeinfo->tm_hour,
              (int) timeinfo->tm_min,
              (int) timeinfo->tm_sec);

  bool res = mgos_mqtt_pub(topic, message, strlen(message), 1, false);
  LOG(LL_INFO, ("Published to MQTT: %s", res ? "yes" : "no"));

  snprintf(action_passed, sizeof(action_passed), action.p);
  pump_action(action_passed);

  (void) payload;
  (void) userdata;
}

static void led_cb(char action[5]) {
  bool action_bool = 1;
  if (strcmp(action, "toggle") == 0)
  {
    action_bool = mgos_gpio_toggle(LED1_PIN);
  }
  else {
    if (strcmp(action, "on") == 0)
    {
      action_bool = 0;
    }
    else if (strcmp(action, "off") == 0)
    {
      action_bool = 1;
    }
    mgos_gpio_write(LED1_PIN, action_bool);
  }

  //s_led_reading = action_bool;
  report_state();
  LOG(LL_INFO, ("LED1 set -> %s", action));
}

static void button_cb(int pin, void *arg) {
  char buf[8];
  mgos_gpio_toggle(mgos_sys_config_get_board_led3_pin());
  LOG(LL_INFO, ("Pin: %s, button pushed", mgos_gpio_str(pin, buf)));
  pump_action("toggle");
  (void) arg;
}

static void timer_cb(void *arg) {
  LOG(LL_INFO, ("uptime: %.2lf, RAM: %lu, %lu free", mgos_uptime(),
                (unsigned long) mgos_get_heap_size(),
                (unsigned long) mgos_get_free_heap_size()));
  (void) arg;
}

static void net_cb(int ev, void *evd, void *arg) {
  switch (ev) {
    case MGOS_NET_EV_DISCONNECTED:
      LOG(LL_INFO, ("%s", "Net disconnected"));
      break;
    case MGOS_NET_EV_CONNECTING:
      LOG(LL_INFO, ("%s", "Net connecting..."));
      break;
    case MGOS_NET_EV_CONNECTED:
      LOG(LL_INFO, ("%s", "Net connected"));
      break;
    case MGOS_NET_EV_IP_ACQUIRED:
      LOG(LL_INFO, ("%s", "Net got IP address"));
      break;
  }

  (void) evd;
  (void) arg;
}

#ifdef MGOS_HAVE_WIFI
static void wifi_cb(int ev, void *evd, void *arg) {
  switch (ev) {
    case MGOS_WIFI_EV_STA_DISCONNECTED:
      LOG(LL_INFO, ("WiFi STA disconnected %p", arg));
      led_cb("off");
      break;
    case MGOS_WIFI_EV_STA_CONNECTING:
      LOG(LL_INFO, ("WiFi STA connecting %p", arg));
      led_cb("toggle");
      break;
    case MGOS_WIFI_EV_STA_CONNECTED:
      LOG(LL_INFO, ("WiFi STA connected %p", arg));
      led_cb("toggle");
      break;
    case MGOS_WIFI_EV_STA_IP_ACQUIRED:
      LOG(LL_INFO, ("WiFi STA IP acquired %p", arg));
      led_cb("on");
      break;
    case MGOS_WIFI_EV_AP_STA_CONNECTED: {
      struct mgos_wifi_ap_sta_connected_arg *aa =
          (struct mgos_wifi_ap_sta_connected_arg *) evd;
      LOG(LL_INFO, ("WiFi AP STA connected MAC %02x:%02x:%02x:%02x:%02x:%02x",
                    aa->mac[0], aa->mac[1], aa->mac[2], aa->mac[3], aa->mac[4],
                    aa->mac[5]));
      break;
    }
    case MGOS_WIFI_EV_AP_STA_DISCONNECTED: {
      struct mgos_wifi_ap_sta_disconnected_arg *aa =
          (struct mgos_wifi_ap_sta_disconnected_arg *) evd;
      LOG(LL_INFO,
          ("WiFi AP STA disconnected MAC %02x:%02x:%02x:%02x:%02x:%02x",
           aa->mac[0], aa->mac[1], aa->mac[2], aa->mac[3], aa->mac[4],
           aa->mac[5]));
      break;
    }
  }
  (void) arg;
}
#endif /* MGOS_HAVE_WIFI */

enum mgos_app_init_result mgos_app_init(void) {

  char buf[8];
  /* Setup device shadown handler */
  mgos_event_add_group_handler(MGOS_SHADOW_BASE, shadow_cb, NULL);

  /* Initilize LED, buttons and GPIO pins */
  LOG(LL_INFO, ("LED pin %s", mgos_gpio_str(LED1_PIN, buf)));
  mgos_gpio_set_mode(LED1_PIN, MGOS_GPIO_MODE_OUTPUT);

  LOG(LL_INFO, ("RELAY pin %s", mgos_gpio_str(RELAY1_PIN, buf)));
  mgos_gpio_set_mode(RELAY1_PIN, MGOS_GPIO_MODE_OUTPUT);

  /* Heartbeat timer */
  mgos_set_timer(10000, MGOS_TIMER_REPEAT, timer_cb, NULL);

  /* Crontab handlers */
  mgos_crontab_register_handler(mg_mk_str("on"), pump_cb, NULL);
  mgos_crontab_register_handler(mg_mk_str("off"), pump_cb, NULL);

  /* Publish to MQTT on button press */
  int btn_pin = BUTTON1_PIN;
  if (btn_pin >= 0) {
    enum mgos_gpio_pull_type btn_pull;
    enum mgos_gpio_int_mode btn_int_edge;
    if (mgos_sys_config_get_board_btn1_pull_up()) {
      btn_pull = MGOS_GPIO_PULL_UP;
      btn_int_edge = MGOS_GPIO_INT_EDGE_NEG;
    } else {
      btn_pull = MGOS_GPIO_PULL_DOWN;
      btn_int_edge = MGOS_GPIO_INT_EDGE_POS;
    }
    LOG(LL_INFO, ("Button pin %s, active %s", mgos_gpio_str(btn_pin, buf),
                  (mgos_sys_config_get_board_btn1_pull_up() ? "low" : "high")));
    mgos_gpio_set_button_handler(btn_pin, btn_pull, btn_int_edge, 20, button_cb,
                                 NULL);
  }

  /* Network connectivity events */
  mgos_event_add_group_handler(MGOS_EVENT_GRP_NET, net_cb, NULL);

#ifdef MGOS_HAVE_WIFI
  mgos_event_add_group_handler(MGOS_WIFI_EV_BASE, wifi_cb, NULL);
#endif

  return MGOS_APP_INIT_SUCCESS;
}
