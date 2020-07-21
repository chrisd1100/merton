let WASM_MODULE;
let GL_CONTEXT;

function mem() {
	return new DataView(WASM_MODULE.instance.exports.memory.buffer);
}

function mem_raw() {
	return WASM_MODULE.instance.exports.memory.buffer;
}

function setUint32(ptr, value) {
	mem().setUint32(ptr, value, true);
}

function setUint64(ptr, value) {
	mem().setBigUint64(ptr, BigInt(value), true);
}

function func_ptr(ptr) {
	return WASM_MODULE.instance.exports.__indirect_function_table.get(ptr);
}

function c_to_js(ptr) {
	const view = new Uint8Array(mem_raw(), ptr);
	let str = '';

	for (let x = 0; x < 0x7FFFFFFF; x++) {
		if (view[x] == 0)
			break;

		str += String.fromCharCode(view[x]);
	}

	return str;
}

export async function MTY_Start(bin) {
	// Fetch the wasm file as an ArrayBuffer
	const res = await fetch(bin);
	const buf = await res.arrayBuffer();

	// Create wasm instance (module) from the ArrayBuffer
	WASM_MODULE = await WebAssembly.instantiate(buf, {
		// Custom imports
		env: {
			// Audio
			MTY_AudioCreate: function () {},
			MTY_AudioDestroy: function () {},
			MTY_AudioPlay: function () {},
			MTY_AudioStop: function () {},
			MTY_AudioQueue: function () {},
			MTY_AudioIsPlaying: function () {},
			MTY_AudioGetQueuedFrames: function () {},

			// Window
			MTY_WindowPoll: function () {},
			MTY_WindowSetWindowed: function () {},
			MTY_WindowSetFullscreen: function () {},
			MTY_WindowGetRefreshRate: function () {
				return 60;
			},
			MTY_GLGetProcAddress: function () {},
			MTY_WindowIsFullscreen: function () {},
			MTY_WindowRenderQuad: function () {},
			MTY_WindowIsForeground: function () {},
			MTY_WindowGetDevice: function () {
				return 0;
			},
			MTY_WindowGetContext: function () {
				return 0;
			},
			MTY_WindowGetBackBuffer: function () {
				return 0;
			},
			MTY_WindowGetDPIScale: function () {
				return 1.0;
			},
			MTY_WindowPresent: function () {},
			MTY_WindowDestroy: function () {},
			MTY_WindowSetTitle: function (ctx, title, subtitle) {
				const js_title = c_to_js(title);
				const js_subittle = subtitle ? c_to_js(subtitle) : '';

				document.title = js_title + (js_subtitle ? ' - ' + js_subtitle : '');
			},
			MTY_WindowCreate: function (title, msg_func, opaque, width, height, fullscreen, window_out) {
				document.title = c_to_js(title);

				const body = document.querySelector('body');
				body.style.width = '100%';
				body.style.height = '100%';
				body.style.padding = 0;
				body.style.margin = 0;

				const canvas = document.createElement('canvas');
				document.body.appendChild(canvas);

				GL_CONTEXT = canvas.getContext('webgl');

				GL_CONTEXT.viewport(0, 0, GL_CONTEXT.drawingBufferWidth, GL_CONTEXT.drawingBufferHeight);
				GL_CONTEXT.clearColor(0.0, 0.5, 0.0, 1.0);
				GL_CONTEXT.clear(GL_CONTEXT.COLOR_BUFFER_BIT);

				return true;
			},
			MTY_AppRun: function (func, opaque) {
				const step = (ts) => {
					func_ptr(func)(opaque);
					window.requestAnimationFrame(step);
				};

				window.requestAnimationFrame(step);
			},
		},

		// Current version of WASI we're compiling against, 'wasi_snapshot_preview1'
		wasi_snapshot_preview1: {
			args_get: function () {
				console.log('args_get:', arguments);
				return 0;
			},
			args_sizes_get: function (argc, argv_buf_size) {
				console.log('args_sizes_get:', arguments);
				setUint32(argc, 0);
				setUint32(argv_buf_size, 0);
				return 0;
			},
			clock_time_get: function (id, precision, time_out) {
				setUint64(time_out, Math.trunc(performance.now() * 1000.0 * 1000.0));
				return 0;
			},
			fd_close: function () {
				console.log('fd_close:', arguments);
			},
			fd_fdstat_get: function () {
				console.log('fd_fdstat_get:', arguments);
			},
			fd_fdstat_set_flags: function () {
				console.log('fd_fdstat_set_flags:', arguments);
			},
			fd_prestat_dir_name: function (fd, path, path_len) {
				console.log('fd_prestat_dir_name:', arguments);
				return 28;
			},
			fd_prestat_get: function (fd, path) {
				console.log('fd_prestat_get:', arguments);
				return 8;
			},
			fd_read: function () {
				console.log('fd_read:', arguments);
			},
			fd_readdir: function () {
				console.log('fd_readdir:', arguments);
			},
			fd_seek: function () {
				console.log('fd_seek:', arguments);
			},
			fd_write: function (fd, iovs, iovs_len, nwritten) {
				console.log('fd_write:', arguments);

				let buffers = Array.from({length: iovs_len}, function (_, i) {
				   let ptr = iovs + i * 8;
				   let buf = mem().getUint32(ptr, !0);
				   let bufLen = mem().getUint32(ptr + 4, !0);

				   return new Uint8Array(mem_raw(), buf, bufLen);
				});

				let written = 0;
				let bufferBytes = [];
				for (let x = 0; x < buffers.length; x++) {
					for (var b = 0; b < buffers[x].byteLength; b++)
					   bufferBytes.push(buffers[x][b]);

					written += buffers[x].byteLength;
				}

				if (fd == 1)
					console.log(String.fromCharCode.apply(null, bufferBytes));

				mem().setUint32(nwritten, written, true);

				return 0;
			},
			path_create_directory: function () {
				console.log('path_create_directory:', arguments);
			},
			path_open: function () {
				console.log('path_open:', arguments);
			},
			path_readlink: function () {
				console.log('path_readlink:', arguments);
			},
			poll_oneoff: function (sin, sout, nsubscriptions, nevents) {
				console.log('poll_oneoff:', arguments);
				return 0;
			},
			proc_exit: function () {
				console.log('proc_exit:', arguments);
			},
		},
	});

	// Execute the '_start' entry point, this will fetch args and execute the 'main' function
	WASM_MODULE.instance.exports._start();
}
