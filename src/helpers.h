#ifndef _HELPERS_PCH
#define _HELPERS_PCH

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

#if defined(__x86_64__)
#include <cpuid.h>
#endif

#include <algorithm>
#include <string>
#include <thread>
#include <vector>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <aes.hpp>
#include <database/sprite.hpp>
#include <default.hpp>
#include <deflate.hpp>
#include <divafile.hpp>
#include <f2/enrs.hpp>
#include <f2/header.hpp>
#include <f2/pof.hpp>
#include <f2/struct.hpp>
#include <farc.hpp>
#include <half_t.hpp>
#include <hash.hpp>
#include <image.hpp>
#include <interpolation.hpp>
#include <io/file_stream.hpp>
#include <io/memory_stream.hpp>
#include <io/path.hpp>
#include <io/stream.hpp>
#include <key_val.hpp>
#include <kf.hpp>
#include <mat.hpp>
#include <prj/algorithm.hpp>
#include <prj/math.hpp>
#include <prj/shared_ptr.hpp>
#include <prj/stack_allocator.hpp>
#include <prj/time.hpp>
#include <prj/vector_pair.hpp>
#include <prj/vector_pair_combine.hpp>
#include <quat.hpp>
#include <rectangle.hpp>
#include <sort.hpp>
#include <spr.hpp>
#include <str_utils.hpp>
#include <time.hpp>
#include <txp.hpp>
#include <types.hpp>
#include <vec.hpp>
#include <waitable_timer.hpp>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

#define PYTHON_TYPE_DEF(original_type)                                                                      \
	static PyType_Spec pytypespec_##original_type = {.name      = "KKdLib." #original_type,                 \
	                                                 .basicsize = sizeof (pyobject_##original_type),        \
	                                                 .itemsize  = 0,                                        \
	                                                 .flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE, \
	                                                 .slots     = pyslots_##original_type};                     \
	PyTypeObject *pytype_##original_type          = nullptr;

#define PYTHON_TYPE_INIT(original_type)                                                     \
	pytype_##original_type = (PyTypeObject *)PyType_FromSpec (&pytypespec_##original_type); \
	PyModule_AddObjectRef (m, #original_type, (PyObject *)pytype_##original_type);

#endif
