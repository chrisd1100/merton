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
		// console.log('1');
		for (let x = 0; x < n; x++)
			setUint32(ids + x * 4, gl_new(GL.createFramebuffer()));
	},
	glDeleteFramebuffers: function (n, ids) {
		// console.log('2');
		for (let x = 0; x < n; x++)
			GL.deleteFramebuffer(gl_del(getUint32(ids + x * 4)));
	},
	glBindFramebuffer: function (target, fb) {
		//console.log('3');
		GL.bindFramebuffer(target, fb ? gl_obj(fb) : null);
	},
	glBlitFramebuffer: function (srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter) {
		console.log('4');
		GL.blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
	},
	glFramebufferTexture2D: function (target, attachment, textarget, texture, level) {
		// console.log('5');
		GL.framebufferTexture2D(target, attachment, textarget, gl_obj(texture), level);
	},
	glEnable: function (cap) {
		console.log('6');
		GL.enable(cap);
	},
	glIsEnabled: function (cap) {
		// console.log('7');
		return GL.isEnabled(cap);
	},
	glDisable: function (cap) {
		//console.log('8');
		GL.disable(cap);
	},
	glViewport: function (x, y, width, height) {
		//console.log('9');
		GL.viewport(x, y, width, height);
	},
	glGetIntegerv: function (name, data) {
		// console.log('10');
		// XXX these can be objects
		const p = GL.getParameter(name);
		setUint32(data, p);
	},
	glGetFloatv: function (name, data) {
		// console.log('11');
		setFloat(data, GL.getParameter(name));
	},
	glBindTexture: function (target, texture) {
		//console.log('12');
		GL.bindTexture(target, texture ? gl_obj(texture) : null);
		GL_BOUND_TEX = texture;
	},
	glDeleteTextures: function () {
		console.log('13');
	},
	glGetTexLevelParameteriv: function (target, level, pname, params) {
		// console.log('14');

		switch (pname) {
			case 0x1000: // Width
				setUint32(params, GL_TEX[target].x;
				break;
			case 0x1001: // Height
				setUint32(params, GL_TEX[target].y;
				break;
		}
	},
	glTexParameteri: function (target, pname, param) {
		// console.log('15');
		GL.texParameteri(target, pname, param);
	},
	glGenTextures: function (n, ids) {
		// console.log('16');
		for (let x = 0; x < n; x++)
			setUint32(ids + x * 4, gl_new(GL.createTexture()));
	},
	glTexImage2D: function (target, level, internalformat, width, height, border, format, type, data) {
		// console.log('17');
		GL.texImage2D(target, level, internalformat, width, height, border, format, type,
			new Uint8Array(mem_raw(), data));

		if (!GL_TEX[GL_BOUND_TEX])
			GL_TEX[GL_BOUND_TEX] = {};

		GL_TEX[GL_BOUND_TEX].x = width;
		GL_TEX[GL_BOUND_TEX].y = height;
	},
	glTexSubImage2D: function () {
		console.log('18');
	},
	glDrawElements: function () {
		console.log('19');
	},
	glGetAttribLocation: function (program, c_name) {
		// console.log('20');
		return GL.getAttribLocation(gl_obj(program), c_to_js(name));
	},
	glShaderSource: function (shader, count, c_strings, c_len) {
		//console.log('21');

		let source = '';
		for (let x = 0; x < count; x++)
			source += c_to_js(getUint32(c_strings + x * 4));

		GL.shaderSource(gl_obj(shader), source);
	},
	glBindBuffer: function (target, buffer) {
		//console.log('22');
		GL.bindBuffer(target, buffer ? gl_obj(buffer) : null);
	},
	glVertexAttribPointer: function () {
		console.log('23');
	},
	glCreateProgram: function () {
		// console.log('24');
		return gl_new(GL.createProgram());
	},
	glUniform1i: function () {
		console.log('25');
	},
	glActiveTexture: function (texture) {
		// console.log('26');
		GL.activeTexture(texture);
	},
	glDeleteBuffers: function () {
		console.log('27');
	},
	glEnableVertexAttribArray: function () {
		console.log('28');
	},
	glBufferData: function (target, size, data, usage) {
		// console.log('29');
		GL.bufferData(target, new Uint8Array(mem_raw(), data, size), usage);
	},
	glDeleteShader: function () {
		console.log('30');
	},
	glGenBuffers: function (n, ids) {
		// console.log('31');
		for (let x = 0; x < n; x++)
			setUint32(ids + x * 4, gl_new(GL.createBuffer()));
	},
	glCompileShader: function (shader) {
		// console.log('32');
		GL.compileShader(gl_obj(shader));
	},
	glLinkProgram: function (program) {
		// console.log('33');
		GL.linkProgram(gl_obj(program));
	},
	glGetUniformLocation: function (program, name) {
		// console.log('34');
		return GL.getUniformLocation(gl_obj(program), c_to_js(name));
	},
	glCreateShader: function (type) {
		// console.log('35');
		return gl_new(GL.createShader(type));
	},
	glAttachShader: function (program, shader) {
		// console.log('36');
		GL.attachShader(gl_obj(program), gl_obj(shader));
	},
	glUseProgram: function (program) {
		//console.log('37');
		GL.useProgram(program ? gl_obj(program) : null);
	},
	glGetShaderiv: function (shader, pname, params) {
		//console.log('38');
		if (pname == 0x8B81) {
			let ok = GL.getShaderParameter(gl_obj(shader), GL.COMPILE_STATUS);
			setUint32(params, ok);

			if (!ok)
				console.warn(GL.getShaderInfoLog(gl_obj(shader)));
		}
	},
	glDetachShader: function () {
		console.log('39');
	},
	glDeleteProgram: function () {
		console.log('40');
	},
	glClear: function () {
		console.log('41');
	},
	glClearColor: function (red, green, blue, alpha) {
		//console.log('42');
		GL.clearColor(red, green, blue, alpha);
	},
	glGetError: function () {
		// console.log('43');
		return GL.getError();
	},
	glGetShaderInfoLog: function () {
		console.log('44');
	},
	glFinish: function () {
		console.log('45');
	},
	glScissor: function () {
		console.log('46');
	},
	glBlendFunc: function () {
		console.log('47');
	},
	glBlendEquation: function () {
		console.log('48');
	},
	glUniformMatrix4fv: function () {
		console.log('49');
	},
	glBlendEquationSeparate: function () {
		console.log('50');
	},
	glBlendFuncSeparate: function () {
		console.log('51');
	},
	glGetProgramiv: function (program, pname, params) {
		// console.log('52');
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

		GL = CANVAS.getContext('webgl2');
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
		console.log('poll_oneoff:', arguments);
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
