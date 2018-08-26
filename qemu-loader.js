"use strict";

function config_update_progress() {
  var unknown = false;
  var loaded = 0;
  var total = 0;
  for (var i = 0; i < config_xhr.length; ++i) {
    var progress = config_xhr[i].last_progress;
    if (!progress || !progress.lengthComputable) {
      unknown = true;
    } else {
      loaded += progress.loaded;
      total += progress.total;
    }
  }
  if (unknown) {
    Module.setStatus('Loading...');
  } else {
    Module.setStatus('Loaded ' + ((loaded / 1e6)|0) + ' Mb (' + loaded + '/' + total + ')');
  }
}

function config_load_file(name, format, onload) {
  var xhr = new XMLHttpRequest();

  if (window.config_xhr === false) {
    return; // an error already occurred
  }
  if (!window.config_xhr) {
    window.config_xhr = [];
    config_xhr.done = 0;
  }
  config_xhr.push(xhr);

  xhr._real_onload = onload;
  xhr.addEventListener('progress', function(event) {
    xhr.last_progress = event;
    config_update_progress();
  });
  function loaded(event) {
    if (config_xhr === false)
      return;
    config_xhr.done += 1;
    if (config_xhr.length == config_xhr.done && window.config_on_load) {
      Module.setStatus('');
      for (var i = 0; i < config_xhr.length; ++i) {
        config_xhr[i]._real_onload(config_xhr[i].response);
      }
      config_on_load();
      window.config_xhr = null;
      window.config_on_load = null;
    }
  };
  function failed(event) {
    for (var i = 0; i < config_xhr.length; ++i)
      config_xhr[i].abort();
    window.config_xhr = false;
    Module.setStatus('Cannot download ' + name);
  };
  xhr.addEventListener('load', loaded);
  xhr.addEventListener('error', failed);
  xhr.open('GET', name);
  xhr.responseType = format;
  xhr.send(null);
}

function initialize_qemu_shell() {
  config_load_file('presets.json', 'json', function(response) {
    window.qemu_presets = response;
  });
  config_load_file('stats.json', 'json', function(response) {
    window.qemu_stats = response;
  });
  window.config_on_load = update_menu;
}

function initialize_size_hints() {

}

function fixup_opts(opts) {
  if (Array.isArray(opts)) {
    opts = {
      "_type": "coll",
      "vals": opts
    };
  }
  if (!opts['_type']) {
    opts['_type'] = 'leaf';
  }
  return opts;
}

function create_config_node(elem) {
  var opts = elem._opts;
  var type = opts['_type'];

  var title = opts['_title'];
  if (title) {
    var titleElement = document.createElement('span');
    titleElement.innerHTML = title;
    elem.appendChild(titleElement);
  }

  if (type == 'select') {
    var selectElement = document.createElement('select');
    selectElement.onchange = function() { config_node_changed(elem); }
    var keys = Object.keys(opts);
    for (var i = 0; i < keys.length; ++i) {
      var k = keys[i];
      if (k[0] == '_')
        continue;

      var opt = new Option(k);
      opt.selected = i == 0;
      opt._val = opts[k];
      selectElement.add(opt);
    }
    elem._select = selectElement;
    elem.append(selectElement);
    elem._content = document.createElement('div')
    elem.append(elem._content);
  } else if (type == 'checkbox') {
    var checkboxElement = document.createElement('input');
    checkboxElement.onchange = function() { config_node_changed(elem); }
    checkboxElement.type = 'checkbox';
    checkboxElement.value = title;
    elem._checkbox = checkboxElement;
    elem.append(checkboxElement);
    elem._content = document.createElement('div')
    elem.append(elem._content);
  } else if (type == 'coll') {
    var vals = opts['vals'];
    elem._contents = [];
    for (var i = 0; i < vals.length; ++i) {
      var x = document.createElement('div');
      elem._contents.push(x);
      elem.append(x);
      populate_config_node(x, vals[i]);
    }
  } else if (type == 'leaf') {
    // do nothing
  } else {
    Module.printErr('Unknown config node type: ' + type);
  }
  config_node_changed(elem);
}

function config_node_changed(elem) {
  var opts = elem._opts;
  if (!opts) {
    Module.printErr('Updating config node without _opts!');
    return;
  }
  var type = opts['_type'];

  if (type == 'select') {
    var selected = elem._select.options[elem._select.selectedIndex];
    populate_config_node(elem._content, selected._val);
  } else if (type == 'checkbox') {
    populate_config_node(elem._content, elem._checkbox.checked ? opts['iftrue'] : opts['iffalse']);
  }
}

function populate_config_node(elem, opts, isRoot) {
  opts = fixup_opts(opts);
  if (elem._opts != opts) {
    while (elem.firstChild) { elem.removeChild(elem.firstChild); }
    elem._opts = opts;
    if (opts) {
      create_config_node(elem);
    }
    if (isRoot) {
      var runButtonElement = document.createElement('input');
      runButtonElement.type = 'button';
      runButtonElement.value = 'Download & run';
      runButtonElement.onclick = run_program;
      elem.append(runButtonElement);
    }
  }
}

function update_menu() {
  var rootElement = document.getElementById('config__root');
  populate_config_node(rootElement, qemu_presets['root'], true);
}

function config_parameters() {
  var rootElement = document.getElementById('config__root');
  var args = [];
  var files = [];
  var js = '';
  function recur(elem) {
    var i;
    var opts = elem._opts;
    if (opts && opts['_type'] == 'leaf') {
      if (opts.files) {
        for (i = 0; i < opts.files.length; ++i)
          files.push(opts.files[i]);
      }
      if (opts.args) {
        for (i = 0; i < opts.args.length; ++i)
          args.push(opts.args[i]);
      }
      if (opts.js)
        js = opts.js;
    } else {
      if (elem._content) {
        recur(elem._content);
      }
      if (elem._contents) {
        for (var i = 0; i < elem._contents.length; ++i) {
          recur(elem._contents[i]);
        }
      }
    }
  }
  recur(rootElement);
  return {
    'js': js,
    'files': files,
    'args': args
  }
}

function config_load_js(jsname) {
  config_load_file(jsname, 'text', function(text) {
    var script = document.createElement('script');
    script.text = text;
    document.body.appendChild(script);
  });
}

function config_preload_file(name) {
  var fname = "//" + name;
  var ind = fname.lastIndexOf('/');
  config_load_file(name, 'arraybuffer', function(data) {
    FS.createPath(
      '/',
      fname.substr(0, ind),
      true,
      true);
    FS.createDataFile(
      fname.substr(0, ind),
      fname.substr(ind + 1),
      new Int8Array(data),
      true, true, true
    )
  });
}

function run_program() {
  var how_to_run = config_parameters();
  Module.printErr(JSON.stringify(how_to_run));
  var files = how_to_run.files;
  config_load_js(how_to_run.js);
  for (var i = 0; i < files.length; ++i) {
    config_preload_file(files[i]);
  }
  window.config_on_load = function() {
    document.getElementById('startup').style.display = 'none';
    document.getElementById('started').style.display = 'block';

    Module.onRuntimeInitialized = function () {
        Module.callMain(how_to_run.args);
    };
  };
}

initialize_qemu_shell();
