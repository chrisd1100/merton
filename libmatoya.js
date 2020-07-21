let MODULE;
let CANVAS;
let GL;

let GL_OBJ_INDEX = 0;
let GL_OBJ = {};
let GL_TEX = {};
let GL_BOUND_TEX = 0;

function mem() {
	return new DataView(MODULE.instance.exports.memory.buffer);
}

function mem_raw() {
	return MODULE.instance.exports.memory.buffer;
}

function malloc(size) {
	return MODULE.instance.exports.malloc(size);
}

function free(cptr) {
	return MODULE.instance.exports.free(cptr);
}

function copy(cptr, abuffer) {
	let heap = new Uint8Array(mem_raw(), cptr);
	heap.set(abuffer);
}

function setUint32(ptr, value) {
	mem().setUint32(ptr, value, true);
}

function setFloat(ptr, value) {
	mem().setFloat32(ptr, value, true);
}

function setUint64(ptr, value) {
	mem().setBigUint64(ptr, BigInt(value), true);
}

function getUint32(ptr) {
	return mem().getUint32(ptr, true);
}

function func_ptr(ptr) {
	return MODULE.instance.exports.__indirect_function_table.get(ptr);
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

function gl_new(obj) {
	GL_OBJ[GL_OBJ_INDEX] = obj;

	return GL_OBJ_INDEX++;
}

function gl_del(index) {
	let obj = GL_OBJ[index];

	GL_OBJ[index] = undefined;
	delete GL_OBJ[index];

	return obj;
}

function gl_obj(index) {
	return GL_OBJ[index];
}

const GL_API = {
	glGenFramebuffers: function (n, ids) {
		for (let x = 0; x < n; x++)
			setUint32(ids + x * 4, gl_new(GL.createFramebuffer()));
	},
	glDeleteFramebuffers: function (n, ids) {
		for (let x = 0; x < n; x++)
			GL.deleteFramebuffer(gl_del(getUint32(ids + x * 4)));
	},
	glBindFramebuffer: function (target, fb) {
		GL.bindFramebuffer(target, fb ? gl_obj(fb) : null);
	},
	glBlitFramebuffer: function (srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter) {
		GL.blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
	},
	glFramebufferTexture2D: function (target, attachment, textarget, texture, level) {
		GL.framebufferTexture2D(target, attachment, textarget, gl_obj(texture), level);
	},
	glEnable: function (cap) {
		GL.enable(cap);
	},
	glIsEnabled: function (cap) {
		return GL.isEnabled(cap);
	},
	glDisable: function (cap) {
		GL.disable(cap);
	},
	glViewport: function (x, y, width, height) {
		GL.viewport(x, y, width, height);
	},
	glGetIntegerv: function (name, data) {
		// FIXME these can be objects and arrays
		const p = GL.getParameter(name);
		setUint32(data, p);
	},
	glGetFloatv: function (name, data) {
		// FIXME these can be objects and arrays
		setFloat(data, GL.getParameter(name));
	},
	glBindTexture: function (target, texture) {
		GL.bindTexture(target, texture ? gl_obj(texture) : null);
		GL_BOUND_TEX = texture;
	},
	glDeleteTextures: function (n, ids) {
		for (let x = 0; x < n; x++)
			GL.deleteTexture(gl_del(getUint32(ids + x * 4)));
	},
	glGetTexLevelParameteriv: function (target, level, pname, params) {
		switch (pname) {
			case 0x1000: // Width
				setUint32(params, GL_TEX[GL_BOUND_TEX].x);
				break;
			case 0x1001: // Height
				setUint32(params, GL_TEX[GL_BOUND_TEX].y);
				break;
		}
	},
	glTexParameteri: function (target, pname, param) {
		GL.texParameteri(target, pname, param);
	},
	glGenTextures: function (n, ids) {
		for (let x = 0; x < n; x++)
			setUint32(ids + x * 4, gl_new(GL.createTexture()));
	},
	glTexImage2D: function (target, level, internalformat, width, height, border, format, type, data) {
		GL.texImage2D(target, level, internalformat, width, height, border, format, type,
			new Uint8Array(mem_raw(), data));

		if (!GL_TEX[GL_BOUND_TEX])
			GL_TEX[GL_BOUND_TEX] = {};

		GL_TEX[GL_BOUND_TEX].x = width;
		GL_TEX[GL_BOUND_TEX].y = height;
	},
	glTexSubImage2D: function (target, level, xofset, yoffset, width, height, format, type, pixels) {
		GL.texSubImage2D(target, level, xoffset, yoffset, width, height, format, type,
			new Uint8Array(mem_raw(), pixels));
	},
	glDrawElements: function (mode, count, type, indices) {
		GL.drawElements(mode, count, type, indices);
	},
	glGetAttribLocation: function (program, c_name) {
		return GL.getAttribLocation(gl_obj(program), c_to_js(c_name));
	},
	glShaderSource: function (shader, count, c_strings, c_len) {
		let source = '';
		for (let x = 0; x < count; x++)
			source += c_to_js(getUint32(c_strings + x * 4));

		GL.shaderSource(gl_obj(shader), source);
	},
	glBindBuffer: function (target, buffer) {
		GL.bindBuffer(target, buffer ? gl_obj(buffer) : null);
	},
	glVertexAttribPointer: function (index, size, type, normalized, stride, pointer) {
		GL.vertexAttribPointer(index, size, type, normalized, stride, pointer);
	},
	glCreateProgram: function () {
		return gl_new(GL.createProgram());
	},
	glUniform1i: function (loc, v0) {
		GL.uniform1i(gl_obj(loc), v0);
	},
	glActiveTexture: function (texture) {
		GL.activeTexture(texture);
	},
	glDeleteBuffers: function () {
		for (let x = 0; x < n; x++)
			GL.deleteBuffer(gl_del(getUint32(ids + x * 4)));
	},
	glEnableVertexAttribArray: function (index) {
		GL.enableVertexAttribArray(index);
	},
	glBufferData: function (target, size, data, usage) {
		GL.bufferData(target, new Uint8Array(mem_raw(), data, size), usage);
	},
	glDeleteShader: function (shader) {
		GL.deleteShader(gl_del(shader));
	},
	glGenBuffers: function (n, ids) {
		for (let x = 0; x < n; x++)
			setUint32(ids + x * 4, gl_new(GL.createBuffer()));
	},
	glCompileShader: function (shader) {
		GL.compileShader(gl_obj(shader));
	},
	glLinkProgram: function (program) {
		GL.linkProgram(gl_obj(program));
	},
	glGetUniformLocation: function (program, name) {
		return gl_new(GL.getUniformLocation(gl_obj(program), c_to_js(name)));
	},
	glCreateShader: function (type) {
		return gl_new(GL.createShader(type));
	},
	glAttachShader: function (program, shader) {
		GL.attachShader(gl_obj(program), gl_obj(shader));
	},
	glUseProgram: function (program) {
		GL.useProgram(program ? gl_obj(program) : null);
	},
	glGetShaderiv: function (shader, pname, params) {
		if (pname == 0x8B81) {
			let ok = GL.getShaderParameter(gl_obj(shader), GL.COMPILE_STATUS);
			setUint32(params, ok);

			if (!ok)
				console.warn(GL.getShaderInfoLog(gl_obj(shader)));

		} else {
			setUint32(params, 0);
		}
	},
	glDetachShader: function (program, shader) {
		GL.detachShader(gl_obj(program), gl_obj(shader));
	},
	glDeleteProgram: function (program) {
		GL.deleteProgram(gl_del(program));
	},
	glClear: function (mask) {
		GL.clear(mask);
	},
	glClearColor: function (red, green, blue, alpha) {
		GL.clearColor(red, green, blue, alpha);
	},
	glGetError: function () {
		return GL.getError();
	},
	glGetShaderInfoLog: function () {
		// Logged automatically as part of glGetShaderiv
	},
	glFinish: function () {
		GL.finish();
	},
	glScissor: function (x, y, width, height) {
		GL.scissor(x, y, width, height);
	},
	glBlendFunc: function (sfactor, dfactor) {
		GL.blendFunc(sfactor, dfactor);
	},
	glBlendEquation: function (mode) {
		GL.blendEquation(mode);
	},
	glUniformMatrix4fv: function (loc, count, transpose, value) {
		GL.uniformMatrix4fv(gl_obj(loc), transpose, new Float32Array(mem_raw(), value, 4 * 4 * count));
	},
	glBlendEquationSeparate: function (modeRGB, modeAlpha) {
		GL.blendEquationSeparate(modeRGB, modeAlpha);
	},
	glBlendFuncSeparate: function (srcRGB, dstRGB, srcAlpha, dstAlpha) {
		GL.blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
	},
	glGetProgramiv: function (program, pname, params) {
		setUint32(params, GL.getProgramParameter(gl_obj(program), pname));
	},
};

const MTY_AUDIO_API = {
	MTY_AudioCreate: function () {},
	MTY_AudioDestroy: function () {},
	MTY_AudioPlay: function () {},
	MTY_AudioStop: function () {},
	MTY_AudioQueue: function () {},
	MTY_AudioIsPlaying: function () {},
	MTY_AudioGetQueuedFrames: function () {},
};

const MTY_WEB_API = {
	web_get_size: function (c_width, c_height) {
		setUint32(c_width, window.innerWidth);
		setUint32(c_height, window.innerHeight);
	},
	web_resize_canvas: function () {
		CANVAS.width = window.innerWidth;
		CANVAS.height = window.innerHeight;
	},
	web_set_title: function (title) {
		document.title = c_to_js(title);
	},
	web_create_canvas: function () {
		const html = document.querySelector('html');
		html.style.width = '100%';
		html.style.height = '100%';
		html.style.margin = 0;

		const body = document.querySelector('body');
		body.style.width = '100%';
		body.style.height = '100%';
		body.style.margin = 0;

		CANVAS = document.createElement('canvas');
		document.body.appendChild(CANVAS);

		GL = CANVAS.getContext('webgl2', {depth: 0, antialias: 0});
	},
	web_register_drag: function () {
		CANVAS.addEventListener('drop', (ev) => {
			ev.preventDefault();

			if (ev.dataTransfer.items) {
				for (let x = 0; x < ev.dataTransfer.items.length; x++) {
					if (ev.dataTransfer.items[x].kind == 'file') {
						let file = ev.dataTransfer.items[x].getAsFile();

						const reader = new FileReader();
						reader.addEventListener('loadend', (fev) => {
							if (reader.readyState == 2) {
								let buf = new Uint8Array(reader.result);
								let cmem = malloc(buf.length);
								copy(cmem, buf);
								// Memory is ready
							}
						});
						reader.readAsArrayBuffer(file);
						break;
					}
				}
			}
		});

		CANVAS.addEventListener('dragover', (ev) => {
			ev.preventDefault();
		});
	},
	web_raf: function (func, opaque) {
		const step = () => {
			func_ptr(func)(opaque);
			window.requestAnimationFrame(step);
		};

		window.requestAnimationFrame(step);
		throw 'Halt';
	},
};

const WASI_API = {
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

		if (fd == 2)
			console.error(String.fromCharCode.apply(null, bufferBytes));

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
		//console.log('poll_oneoff:', arguments);
		return 0;
	},
	proc_exit: function () {
		console.log('proc_exit:', arguments);
	},
};

export async function MTY_Start(bin) {
	// Fetch the wasm file as an ArrayBuffer
	const res = await fetch(bin);
	const buf = await res.arrayBuffer();

	// Create wasm instance (module) from the ArrayBuffer
	MODULE = await WebAssembly.instantiate(buf, {
		// Custom imports
		env: {
			...GL_API,
			...MTY_AUDIO_API,
			...MTY_WEB_API,
		},

		// Current version of WASI we're compiling against, 'wasi_snapshot_preview1'
		wasi_snapshot_preview1: {
			...WASI_API,
		},
	});

	// Execute the '_start' entry point, this will fetch args and execute the 'main' function
	MODULE.instance.exports._start();
}
