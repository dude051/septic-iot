#include "mgos.h"
#include "mgos_mqtt.h"
#ifdef MGOS_HAVE_WIFI
#include "mgos_wifi.h"
#endif
#include "mgos_shadow.h"
#include "mgos_crontab.h"

#define LED1_PIN = mgos_sys_config_get_board_LED1_PIN();
#define RELAY1_PIN 14
#define RELAY2_PIN 12
#define BUTTON1_PIN 5
#define FMT "{led: %B, relay1: %B, relay2: %B}"

static bool s_led_reading = false;
static bool s_relay1_reading = false;
static bool s_relay2_reading = false;

static void report_state(void) {
    mgos_shadow_updatef(0, FMT,
                        s_led_reading,
                        s_relay1_reading,
                        s_relay2_reading);
}

static void shadow_cb(int ev, void *evd, void *arg) {

    LOG(LL_INFO, ("Event: %d, version: 0", ev));
    if (ev == MGOS_SHADOW_CONNECTED)
    {
        LOG(LL_INFO, ("Reporting connected state"));
        report_state();
        return;
    }

    if (ev != MGOS_SHADOW_GET_ACCEPTED &&
        ev != MGOS_SHADOW_UPDATE_DELTA)
    {
        return;
    }

    struct mg_str * event_data = (struct mg_str *)evd;

    LOG(LL_INFO, ("Response: %.*s", (int) event_data->len, event_data->p));

    //json_scanf(event_data->p, event_data->len, FMT, &s_led_reading, &s_relay1_reading, &s_relay2_reading);

    if (ev == MGOS_SHADOW_UPDATE_DELTA)
    {
        LOG(LL_INFO, ("Reporting updated state"));
        report_state();
    }

    (void) arg;
}

void pump_action(char action[5]) {
  bool action_bool = 1;
  if (strcmp(action, "toggle") == 0) {
    action_bool = mgos_gpio_toggle(RELAY1_PIN);
  } else {
      if (strcmp(action, "on") == 0) {
        action_bool = 0;
      } else if (strcmp(action, "off") == 0) {
        action_bool = 1;
      }
      mgos_gpio_write(RELAY1_PIN, action_bool);
  }
  s_relay1_reading = action_bool;
  report_state();
  LOG(LL_INFO, ("Relay1 set -> %s", action));
}

void pump_cb(struct mg_str action, struct mg_str payload, void *userdata) {
  time_t t = time(0);
  struct tm* timeinfo = localtime(&t);
  char timestamp[24];
  strftime(timestamp, sizeof (timestamp), "%FT%TZ", timeinfo);
  LOG(LL_INFO, ("%s - crontab pump_cb, action: %.*s", timestamp, action.len, action.p));
  if (strcmp(action.p, "pump_on") == 0) {
    pump_action("on");
  }
  if (strcmp(action.p, "pump_off") == 0) {
    pump_action("off");
  }
  (void) payload;
  (void) userdata;
}

static void led_cb(char action[5]) {
  int LED1_PIN = mgos_sys_config_get_board_LED1_PIN();
  int action_bool = 1;
  if (strcmp(action, "toggle") == 0) {
    action_bool = mgos_gpio_toggle(LED1_PIN);
  } else {
      if (strcmp(action, "on") == 0) {
        action_bool = 0;
      } else if (strcmp(action, "off") == 0) {
        action_bool = 1;
      }
      mgos_gpio_write(LED1_PIN, action_bool);
  }
  s_led_reading = action_bool;
  report_state();
  LOG(LL_INFO, ("LED1 set -> %s", action));
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

static void button_cb(int pin, void *arg) {
  char topic[100];
  snprintf(topic, sizeof(topic), "/devices/%s/events",
           mgos_sys_config_get_device_id());
  bool res = mgos_mqtt_pubf(topic, 0, false /* retain */,
                            "{total_ram: %lu, free_ram: %lu}",
                            (unsigned long) mgos_get_heap_size(),
                            (unsigned long) mgos_get_free_heap_size());
  char buf[8];
  int x = mgos_gpio_toggle(mgos_sys_config_get_board_led3_pin());
  LOG(LL_INFO, ("Pin: %s, published: %s x %d", mgos_gpio_str(pin, buf),
                res ? "yes" : "no", x));
  pump_action("toggle");
  (void) arg;
}

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
  mgos_crontab_register_handler(mg_mk_str("pump_on"), pump_cb, NULL);
  mgos_crontab_register_handler(mg_mk_str("pump_off"), pump_cb, NULL);

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
