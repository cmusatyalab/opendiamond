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

function AutoPause(blasters, max_unexposed, tag_class) {
  // Make sure "new" was used
  if (!(this instanceof arguments.callee)) {
    throw new Error('Constructor called as a function');
  }

  // Normalize a single blaster to an array
  if (!(blasters instanceof Array)) {
    blasters = [blasters];
  }

  // Default parameters
  if (typeof max_unexposed === 'undefined') {
    max_unexposed = 50;
  }
  if (typeof tag_class === 'undefined') {
    tag_class = 'unexposed';
  }

  // Private members
  var selector = '.' + tag_class;
  var change_callback;

  // Private methods
  function is_unexposed(el) {
    return !$(el).is(':visible') ||
        $(el).offset().top > $(document).scrollTop() + $(window).height();
  }

  function update_exposed() {
    var changed = false;
    $(selector).each(function(i, el) {
      if (!is_unexposed(el)) {
        $(el).removeClass(tag_class);
        changed = true;
      }
    });

    var count = $(selector).length;
    if (changed && change_callback) {
      change_callback(count);
    }
    if (count < max_unexposed) {
      $.each(blasters, function(i, blaster) {
        blaster.resume();
      });
    }
  }

  // Connect to window events
  $(window).resize(update_exposed);
  $(window).scroll(update_exposed);

  // Public methods
  this.element_added = function(el) {
    var changed = false;
    if (is_unexposed(el)) {
      $(el).addClass(tag_class);
      changed = true;
    }

    var count = $(selector).length;
    if (changed && change_callback) {
      change_callback(count);
    }
    if (count >= max_unexposed) {
      $.each(blasters, function(i, blaster) {
        blaster.pause();
      });
    }
  };

  this.refresh = function() {
    update_exposed();
  };

  // Callback takes the number of unviewed results
  this.onchange = function(callback) {
    change_callback = callback;
  };
}
