//
// The OpenDiamond Platform for Interactive Search
//
// Copyright (c) 2012 Carnegie Mellon University
// All rights reserved.
//
// This software is distributed under the terms of the Eclipse Public
// License, Version 1.0 which can be found in the file named LICENSE.
// ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
// RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
//

function JSONBlasterSocket(url, search_key) {
  // Make sure "new" was used
  if (!(this instanceof arguments.callee)) {
    throw new Error('Constructor called as a function');
  }

  // Private vars
  var blaster = this;
  var sock = new SockJS(url);
  var handlers = {};
  var connected = false;
  var paused = false;

  // Private methods
  function run_handler(name) {
    if (typeof(handlers[name]) === 'function') {
      var handler_args = Array.prototype.slice.call(arguments, 1)
      handlers[name].apply(blaster, handler_args);
    }
  }

  // Public methods
  this.on = function(event, func) {
    handlers[event] = func;
  };

  this.onopen = function(func) {
    blaster.on('__open', func);
  };

  this.onclose = function(func) {
    blaster.on('__close', func);
  };

  this.emit = function(event, data)  {
    sock.send(JSON.stringify({
      'event': event,
      'data': data
    }));
  };

  this.pause = function() {
    if (connected && !paused) {
      blaster.emit('pause');
    }
    paused = true;
  };

  this.resume = function() {
    if (connected && paused) {
      blaster.emit('resume');
    }
    paused = false;
  };

  this.close = function() {
    sock.close();
  };

  // SockJS callbacks
  sock.onopen = function() {
    connected = true;
    blaster.emit('start', {
      'search_key': search_key
    });
    if (paused) {
      blaster.emit('pause');
    }
    run_handler('__open');
  };

  sock.onmessage = function(ev) {
    var msg = JSON.parse(ev.data);
    run_handler(msg.event, msg.data);
  };

  sock.onclose = function(ev) {
    connected = false;
    run_handler('__close', ev);
  };

  // Default event handlers
  blaster.on('ping', function(data) {
    blaster.emit('pong');
  });

  blaster.on('error', function(data) {
    alert(data.message);
  });
}
