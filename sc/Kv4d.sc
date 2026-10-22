// Kv4d - SuperCollider client for kv4d.
// Install: symlink or copy this file into Platform.userExtensionDir, then recompile.
//
//   ~v = Kv4d(path: "/Users/kurivari/dev/kv4d/build/kv4d.app");
//   ~v.open(size: 20);
//   ~v.preset(\coral);
//   ~v.mode(\slice, 2);
//   ~v.rotate(\xw, 0.3);
//   ~v.onAlive = { |alive, gen| alive.postln };
//   ~v.queryAlive(true);

Kv4d {
	var <addr, <path, <inport, <outport, <pid, aliveDef;
	var <>onAlive;

	classvar modes, seeds;

	*initClass {
		modes = (project: 0, slice: 1, stack: 2, grid: 3);
		seeds = (box: 0, symmetric: 1, tesseract: 2, clear: 3);
	}

	*new { |host = "127.0.0.1", inport = 7000, outport = 57121, path|
		^super.new.init(host, inport, outport, path)
	}

	init { |host, argInport, argOutport, argPath|
		inport = argInport;
		outport = argOutport;
		path = argPath;
		addr = NetAddr(host, inport);
		aliveDef = OSCFunc({ |msg| onAlive.value(msg[1], msg[2]) }, '/kv4d/world/alive', recvPort: outport);
	}

	open { |size = 20, width = 1280, height = 800, fullScreen = false, display = 0|
		var bin = path +/+ "Contents/MacOS/kv4d";
		pid = (bin.shellQuote + "-size" + size + "-screenx" + width + "-screeny" + height
			+ "-full" + fullScreen.binaryValue + "-wmode" + display
			+ "-inport" + inport + "-outport" + outport).unixCmd;
	}

	quit { addr.sendMsg('/kv4d/quit') }
	free { aliveDef.free }

	send { |cmd ... args| addr.sendMsg(*(["/kv4d" ++ cmd] ++ args)) }

	// ---- world ----
	init4d { |x = 16, y, z, w| this.send("/world/init", x, y ? x, z ? x, w ? x) }
	rule { |spec| this.send("/world/rule", spec.asString) }       // "B12-17/S11-20/C2/NM"
	preset { |nameOrIndex| this.send("/world/preset", if(nameOrIndex.isNumber) { nameOrIndex } { nameOrIndex.asString }) }
	continuous { |add = 0.005| this.send("/world/continuous", add) }
	births { |...ranges| this.send("/world/births", *ranges) }     // lo, hi, lo, hi ...
	survivals { |...ranges| this.send("/world/survivals", *ranges) }
	states { |n| this.send("/world/states", n) }
	nhood { |vonNeumann = false| this.send("/world/nhood", vonNeumann.binaryValue) }
	seed { |type = \box, extent = 0.5, density = 0.2| this.send("/world/seed", seeds[type] ? type, extent, density) }
	reseed { this.send("/world/reseed") }
	inject { |x = 0.5, y = 0.5, z = 0.5, w = 0.5, radius = 0.1, density = 0.5| this.send("/world/inject", x, y, z, w, radius, density) }
	rate { |gensPerSecond| this.send("/world/rate", gensPerSecond) }
	pause { |flag = true| this.send("/world/pause", flag.binaryValue) }
	step { this.send("/world/step") }
	interpolate { |flag = true| this.send("/world/interpolate", flag.binaryValue) }
	queryAlive { |flag = true| this.send("/world/query/alive", flag.binaryValue) }

	// ---- view ----
	mode { |m, fade = 1.0| this.send("/view/mode", modes[m] ? m, fade) }
	rotate { |plane, speed| this.send("/view/rotate", plane.asString, speed) }  // \xy \xz \xw \yz \yw \zw
	angle { |plane, radians| this.send("/view/angle", plane.asString, radians) }
	resetRotation { this.send("/view/reset") }
	project { |dist = 2.0| this.send("/view/project", dist) }
	slice { |w = 0, width = 0.035, sweep = 0| this.send("/view/slice", w, width, sweep) }
	grid { |spacing = 1.3| this.send("/view/grid", spacing) }
	extent { |size = 200| this.send("/view/extent", size) }
	points { |flag = true| this.send("/view/geometry", flag.binaryValue) }
	cellScale { |s = 0.75| this.send("/view/cellscale", s) }
	pointSize { |s = 1| this.send("/view/pointsize", s) }
	colors { |a, b| this.send("/view/color", a.red, a.green, a.blue, b.red, b.green, b.blue) }
	alpha { |a = 1| this.send("/view/alpha", a) }
	additive { |flag = true| this.send("/view/blend", flag.binaryValue) }
	threshold { |t = 0| this.send("/view/threshold", t) }
	tesseract { |flag = true| this.send("/view/tesseract", flag.binaryValue) }
	background { |color| this.send("/view/background", color.red, color.green, color.blue) }

	// ---- efx ----
	trails { |decay = 0.88| this.send("/efx/trails", decay) }
	glow { |threshold = 0.6, intensity = 1.2, radius = 6| this.send("/efx/glow", threshold, intensity, radius) }

	// ---- audio ----
	audio { |flag = true, device| if(device.notNil) { this.send("/audio/enable", flag.binaryValue, device) } { this.send("/audio/enable", flag.binaryValue) } }
	gain { |g = 1| this.send("/audio/gain", g) }
	audioMap { |target, amount| this.send("/audio/map", target.asString, amount) }  // \rotate \slice \pulse \inject
	framerate { |fps| this.send("/framerate", fps) }
}
