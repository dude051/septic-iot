load('api_config.js');
load('api_gpio.js');
load('api_shadow.js');
load('api_timer.js');
load('api_sys.js');

let led = Cfg.get('board.led1.pin');           // Built-in LED GPIO number
let relay1 = 14;
let relay2 = 12;
let onhi = Cfg.get('board.led1.active_high');  // LED on when high?
let state = {
  led: false,
  uptime: 0,
  relay1: false,
  relay2: false
};            // Device state
let uptime = null;                              // Initialize when connected
let schedule = null;                              // Initialize when connected

let setLED = function(on) {
  let level = onhi ? on : !on;
  GPIO.write(led, level);
  print('LED status ->', on);
};

let setGPIO = function(relay, on) {
  let level = onhi ? on : !on;
  GPIO.write(relay, level);
  print('RELAY', relay, 'status ->', on);
};

let toggleGPIO = function() {
  let toggleState = GPIO.toggle(relay1);
  state.relay1 = toggleState;  // Synchronise the state
  print('RELAY', relay1, 'toggle ->', toggleState);
};

let reportState = function() {
  state.uptime = Sys.uptime();
  print(JSON.stringify(state));
  Shadow.update(0, state);
};

// Set up Shadow handler to synchronise device state with the shadow state
Shadow.addHandler(function(event, obj) {
  print(event);
  if (event === 'CONNECTED') {
    print('Connected to the device shadow');
    print('  Reporting our current state..');
    Shadow.update(0, state);  // Report current state. This may generate the
                              // delta on the cloud, in which case the
                              // cloud will send UPDATE_DELTA to us


    GPIO.set_mode(led, GPIO.MODE_OUTPUT);
    GPIO.set_mode(relay1, GPIO.MODE_OUTPUT);
    GPIO.set_mode(relay2, GPIO.MODE_OUTPUT);
    setLED(state.led);

    print('  Setting up timer to periodically report device state..');
    if (!uptime) uptime = Timer.set(60000, Timer.REPEAT, reportState, null);

    print('  Setting up Crontab to control pump schedule');
    //let f = ffi('int gpio_write(int, int)');   f(2, 1);
    let Crontab = {
      set: ffi('void mgos_crontab_register_handler(char*, void (*)(char*, char*, userdata), userdata)')
    };
    Crontab.set("on", function(){
      setGPIO(relay1, 0);
    }, null );
    Crontab.set("off", function(){
      setGPIO(relay1, 1);
    }, null );

  } else if (event === 'UPDATE_DELTA') {
    print('GOT DELTA:', JSON.stringify(obj));
    for (let key in obj) {  // Iterate over all keys in delta
      if (key === 'led') {   // We know about the 'on' key. Handle it!
        state.led = obj.led;  // Synchronise the state
        setLED(state.led);   // according to the delta
      } else if (key === 'relay1') {   // We know about the 'on' key. Handle it!
        state.relay1 = obj.relay1;  // Synchronise the state
        setGPIO(relay1, state.relay1);
      } else if (key === 'relay2') {   // We know about the 'on' key. Handle it!
        state.relay2 = obj.relay2;  // Synchronise the state
        setGPIO(relay2, state.relay2);
      } else {
        print('Dont know how to handle key', key);
      }
    }
    Shadow.update(0, state);  // Report our new state, hopefully clearing delta
  }
});
