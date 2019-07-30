--
-- Copyright 2010-2019 Branimir Karadzic. All rights reserved.
-- License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
--

function filesexist(_srcPath, _dstPath, _files)
	for _, file in ipairs(_files) do
		file = path.getrelative(_srcPath, file)
		local filePath = path.join(_dstPath, file)
		if not os.isfile(filePath) then return false end
	end

	return true
end

function overridefiles(_srcPath, _dstPath, _files)

	local remove = {}
	local add = {}
	for _, file in ipairs(_files) do
		file = path.getrelative(_srcPath, file)
		local filePath = path.join(_dstPath, file)
		if not os.isfile(filePath) then return end

		table.insert(remove, path.join(_srcPath, file))
		table.insert(add, filePath)
	end

	removefiles {
		remove,
	}

	files {
		add,
	}
end

function rpProjectBase(_kind, _defines)

	kind (_kind)

	if _kind == "SharedLib" then
		defines {
			"BGFX_SHARED_LIB_BUILD=1",
		}

		links {
			"example-common",
			"bimg",
			"bx",
			"bgfx",
		}

		configuration { "vs20* or mingw*" }
			links {
				"gdi32",
				"psapi",
			}

		configuration { "mingw*" }
			linkoptions {
				"-shared",
			}

		configuration { "linux-*" }
			buildoptions {
				"-fPIC",
			}

		configuration {}
	end

	includedirs {
		path.join(BGFX_DIR, "3rdparty"),
		path.join(BX_DIR,   "include"),
		path.join(BIMG_DIR, "include"),
		path.join(BGFX_DIR, "examples/common"),
		path.join(BGFX_DIR, "include"),
	}

	defines {
		_defines,
	}

	links {
		"bx",
		"bgfx",
	}

	if _OPTIONS["with-glfw"] then
		defines {
			"BGFX_CONFIG_MULTITHREADED=0",
		}
	end

	configuration { "Debug" }
		defines {
			"BGFX_CONFIG_DEBUG=1",
		}

	configuration { "vs* or mingw*", "not durango" }
		includedirs {
			path.join(BGFX_DIR, "3rdparty/dxsdk/include"),
		}

	configuration { "android*" }
		links {
			"EGL",
			"GLESv2",
		}

	configuration { "winstore*" }
		linkoptions {
			"/ignore:4264" -- LNK4264: archiving object file compiled with /ZW into a static library; note that when authoring Windows Runtime types it is not recommended to link with a static library that contains Windows Runtime metadata
		}

	configuration { "*clang*" }
		buildoptions {
			"-Wno-microsoft-enum-value", -- enumerator value is not representable in the underlying type 'int'
			"-Wno-microsoft-const-init", -- default initialization of an object of const type '' without a user-provided default constructor is a Microsoft extension
		}

	configuration { "osx" }
		linkoptions {
			"-framework Cocoa",
			"-framework QuartzCore",
			"-framework OpenGL",
			"-weak_framework Metal",
			"-weak_framework MetalKit",
		}

	configuration { "not linux-steamlink", "not NX32", "not NX64" }
		includedirs {
			-- steamlink has EGL headers modified...
			-- NX has EGL headers modified...
			path.join(BGFX_DIR, "3rdparty/khronos"),
		}

	configuration { "linux-steamlink" }
		defines {
			"EGL_API_FB",
		}

	configuration {}

	includedirs {
		path.join(RP_DIR, "include"),
	}

	files {
		path.join(RP_DIR, "include/**.h"),
		path.join(RP_DIR, "src/**.cpp"),
		path.join(RP_DIR, "src/**.h"),
		path.join(RP_DIR, "scripts/**.natvis"),
	}

	removefiles {
		path.join(RP_DIR, "src/**.bin.h"),
	}

	configuration {}
end

function rpProject(_name, _kind, _defines)
	project ("render_pipeline" .. _name)
	uuid (os.uuid("render_pipeline" .. _name))
	rpProjectBase(_kind, _defines)
	copyLib()
end
