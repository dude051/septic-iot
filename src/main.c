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
#define FMT "{led: %s, relay1: %s, relay2: %s, uptime: %s}"
#define STATE "{led: %Q, relay1: %Q, relay2: %Q, uptime: %Q}"

static char led1_state[6] = "on";
static char relay1_state[6] = "off";
static char relay2_state[6] = "off";

static void report_state(void) {
  char uptime[25];
  sprintf(uptime, "%0.1f", mgos_uptime());
  mgos_shadow_updatef(0, STATE,
                      led1_state,
                      relay1_state,
                      relay2_state,
                      uptime
                    );
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
  LOG(LL_INFO, ("Relay1 set -> %s", action));
  strncpy(relay1_state, action, 6);
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
  report_state();

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

  LOG(LL_INFO, ("LED1 set -> %s", action));
  strncpy(led1_state, action, 6);
}

static void button_cb(int pin, void *arg) {
  char buf[8];
  mgos_gpio_toggle(mgos_sys_config_get_board_led3_pin());
  LOG(LL_INFO, ("Pin: %s, button pushed", mgos_gpio_str(pin, buf)));
  pump_action("toggle");
  report_state();
  (void) arg;
}

static void heartbeat_cb(void *arg) {
  report_state();
  (void) arg;
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
    json_scanf(event_data->p, event_data->len, FMT, &led1_state, &relay1_state, &relay2_state);

    if (ev == MGOS_SHADOW_UPDATE_DELTA)
    {
        LOG(LL_INFO, ("Shawdow: Synchronise state locally"));
        led_cb(led1_state);
        pump_action(relay1_state);

        LOG(LL_INFO, ("Shawdow: Reporting updated state"));
        report_state();
    }

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
  }
  (void) evd;
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
  mgos_set_timer(60000, MGOS_TIMER_REPEAT, heartbeat_cb, NULL);

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
#ifdef MGOS_HAVE_WIFI
  mgos_event_add_group_handler(MGOS_WIFI_EV_BASE, wifi_cb, NULL);
#endif

  return MGOS_APP_INIT_SUCCESS;
}
