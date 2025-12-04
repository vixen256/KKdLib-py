#include "BC.h"
#include "helpers.h"

struct pyobject_farc_file {
	PyObject_HEAD;
	farc_file *real;
};

static int
py_farc_file_init (pyobject_farc_file *self, PyObject *args, PyObject *kwds) {
	const char *name = "DEFAULT";
	PyObject *data   = nullptr;
	char *kwlist[]   = {"name", "data", nullptr};
	if (!PyArg_ParseTupleAndKeywords (args, kwds, "|sO!", kwlist, &name, &PyBytes_Type, &data)) return -1;
	self->real = new farc_file;

	self->real->name.assign (name);
	if (data != nullptr && PyBytes_Check (data)) {
		self->real->size = PyBytes_Size (data);
		self->real->data = malloc (self->real->size);
		memcpy (self->real->data, PyBytes_AsString (data), self->real->size);
	}

	return 0;
}

void
py_farc_file_finalize (pyobject_farc_file *self) {
	delete self->real;
}

static PyObject *
pyrepr_farc_file (pyobject_farc_file *self) {
	return PyUnicode_FromFormat ("farc_file(name=%s, size=%lld)", self->real->name.c_str (), self->real->size);
}

static PyObject *
py_farc_file_get_name (pyobject_farc_file *self, void *closure) {
	return PyUnicode_FromString (self->real->name.c_str ());
}

static int
py_farc_file_set_name (pyobject_farc_file *self, PyObject *value, void *closure) {
	if (value == nullptr || !PyUnicode_Check (value)) {
		PyErr_SetString (PyExc_TypeError, "Name must be string");
		return -1;
	}

	PyObject *bytes = PyUnicode_AsUTF8String (value);
	self->real->name.assign (PyBytes_AsString (bytes));
	Py_DECREF (bytes);

	return 0;
}

static int
py_farc_file_set_data (pyobject_farc_file *self, PyObject *value, void *closure) {
	if (value == nullptr || !PyBytes_Check (value)) {
		PyErr_SetString (PyExc_TypeError, "Data must be bytes");
		return -1;
	}

	self->real->size = PyBytes_Size (value);
	self->real->data = malloc (self->real->size);
	memcpy (self->real->data, PyBytes_AsString (value), self->real->size);

	return 0;
}

static PyGetSetDef pygetsets_farc_file[] = {{"name", (getter)py_farc_file_get_name, (setter)py_farc_file_set_name, "Name", nullptr},
                                            {"data", nullptr, (setter)py_farc_file_set_data, "File data", nullptr},
                                            {nullptr}};

static PyType_Slot pyslots_farc_file[] = {
    {Py_tp_repr, (void *)pyrepr_farc_file},
    {Py_tp_getset, pygetsets_farc_file},
    {Py_tp_init, (void *)py_farc_file_init},
    {Py_tp_finalize, (void *)py_farc_file_finalize},
    {0},
};

PYTHON_TYPE_DEF (farc_file);

struct pyobject_farc {
	PyObject_HEAD;
	farc *real;
};

static int
py_farc_init (pyobject_farc *self, PyObject *args, PyObject *kwds) {
	const char *signature = "FArC";
	bool ft               = true;
	char *kwlist[]        = {"signature", "ft", nullptr};
	if (!PyArg_ParseTupleAndKeywords (args, kwds, "|sb", kwlist, &signature, &ft)) return -1;

	self->real = new farc;

	if (strcmp (signature, "FArc") == 0) {
		self->real->signature = FARC_FArc;
		self->real->flags     = FARC_NONE;
	} else if (strcmp (signature, "FArC") == 0) {
		self->real->signature = FARC_FArC;
		self->real->flags     = FARC_GZIP;
	} else if (strcmp (signature, "FARC") == 0) {
		self->real->signature = FARC_FARC;
		self->real->flags     = (farc_flags)(FARC_GZIP | FARC_AES);
	} else {
		PyErr_SetString (PyExc_RuntimeError, "Signature must be one of [FArc, FarC, FARC]");
		return -1;
	}

	self->real->ft                = ft;
	self->real->compression_level = 12;
	self->real->alignment         = 0x10;
	if (ft) {
		self->real->entry_size  = 0x10;
		self->real->header_size = 0x10;
	}

	return 0;
}

void
py_farc_finalize (pyobject_farc *self) {
	delete self->real;
}

static PyObject *
py_farc_add_file (pyobject_farc *self, PyObject *args) {
	pyobject_farc_file *file;
	if (!PyArg_ParseTuple (args, "O!", pytype_farc_file, &file)) return nullptr;

	auto self_file = self->real->add_file (file->real->name.c_str ());
	*self_file     = *file->real;
	if (self->real->flags & FARC_GZIP) self_file->compressed = true;
	if (self->real->flags & FARC_AES) self_file->encrypted = true;
	// Prevent double free
	file->real->data            = nullptr;
	file->real->data_compressed = nullptr;

	Py_RETURN_NONE;
}

static PyObject *
py_farc_write (pyobject_farc *self, PyObject *args) {
	const char *path;
	if (!PyArg_ParseTuple (args, "s", &path)) return nullptr;

	if (path_check_file_exists (path)) path_delete_file (path);
	self->real->write (path, self->real->signature, self->real->flags, false, false);

	Py_RETURN_NONE;
}

static PyObject *
py_farc_get_files (pyobject_farc *self, void *closure) {
	PyObject *list = PyList_New (self->real->files.size ());

	for (u64 i = 0; i < self->real->files.size (); i++) {
		pyobject_farc_file *obj = PyObject_New (pyobject_farc_file, pytype_farc_file);
		PyObject_Init ((PyObject *)obj, pytype_farc_file);
		obj->real                  = new farc_file;
		*obj->real                 = self->real->files.at (i);
		obj->real->data            = nullptr;
		obj->real->data_compressed = nullptr;
		PyList_SetItem (list, i, (PyObject *)obj);
	}

	return list;
}

static PyMethodDef pymethods_farc[] = {{"add_file", (PyCFunction)py_farc_add_file, METH_VARARGS, "Add file to farc (farc_file)"},
                                       {"write", (PyCFunction)py_farc_write, METH_VARARGS, "Write data to file (path)"},
                                       {nullptr}};

static PyGetSetDef pygetsets_farc[] = {{"files", (getter)py_farc_get_files, nullptr, "Files", nullptr}, {nullptr}};

static PyType_Slot pyslots_farc[] = {
    {Py_tp_methods, pymethods_farc},
    {Py_tp_getset, pygetsets_farc},
    {Py_tp_init, (void *)py_farc_init},
    {Py_tp_finalize, (void *)py_farc_finalize},
    {0},
};

PYTHON_TYPE_DEF (farc);

struct pyobject_txp_set {
	PyObject_HEAD;
	txp_set *real;
	std::vector<std::string> *names;
};

static int
py_txp_set_init (pyobject_txp_set *self, PyObject *args, PyObject *kwds) {
	self->real  = new txp_set ();
	self->names = new std::vector<std::string> ();

	return 0;
}

void
py_txp_set_finalize (pyobject_txp_set *self) {
	delete self->real;
	delete self->names;
}

static PyObject *
py_txp_set_add_texture_data (pyobject_txp_set *self, PyObject *args) {
	const char *name;
	i32 width;
	i32 height;
	const char *format_name;
	PyObject *data;
	if (!PyArg_ParseTuple (args, "siisO!", &name, &width, &height, &format_name, &PyBytes_Type, &data)) return nullptr;

	txp_format format;
	if (strcmp (format_name, "RGB") == 0) {
		format = TXP_RGB8;
	} else if (strcmp (format_name, "RGBA") == 0) {
		format = TXP_RGBA8;
	} else if (strcmp (format_name, "BC1") == 0 || strcmp (format_name, "DXT1") == 0) {
		format = TXP_BC1;
	} else if (strcmp (format_name, "BC2") == 0 || strcmp (format_name, "DXT3") == 0) {
		format = TXP_BC2;
	} else if (strcmp (format_name, "BC3") == 0 || strcmp (format_name, "DXT5") == 0) {
		format = TXP_BC3;
	} else {
		PyErr_SetString (PyExc_RuntimeError, "Format must be one of [RGB, RGBA, BC1/DXT1, BC2/DXT3, BC3/DXT5]");
		return nullptr;
	}

	txp_mipmap mipmap;
	txp texture;

	mipmap.width  = width;
	mipmap.height = height;
	mipmap.format = format;

	if (PyBytes_Size (data) != mipmap.get_size ()) {
		PyErr_SetString (PyExc_RuntimeError, "Data does not match expected size");
		return nullptr;
	}

	mipmap.size = mipmap.get_size ();
	mipmap.data.resize (mipmap.size);
	memcpy (mipmap.data.data (), PyBytes_AsString (data), mipmap.size);

	texture.has_cube_map  = false;
	texture.array_size    = 1;
	texture.mipmaps_count = 1;
	texture.mipmaps.push_back (mipmap);

	self->real->textures.push_back (texture);
	self->names->push_back (std::string (name));

	Py_RETURN_NONE;
}

static PyObject *
py_txp_set_add_texture_pillow (pyobject_txp_set *self, PyObject *args) {
	const char *name;
	PyObject *image;
	const char *format = "ATI2";
	if (!PyArg_ParseTuple (args, "sO|s", &name, &image, &format)) return nullptr;

	PyObject *width  = PyObject_GetAttrString (image, "width");
	PyObject *height = PyObject_GetAttrString (image, "height");
	PyObject *mode   = PyObject_GetAttrString (image, "mode");
	if (width == nullptr || height == nullptr || mode == nullptr || !PyLong_Check (width) || !PyLong_Check (height) || !PyUnicode_Check (mode)) {
		PyErr_SetString (PyExc_RuntimeError, "Could not find image.width");
		return nullptr;
	}

	PyObject *image_data = PyObject_CallMethod (image, "getdata", nullptr);
	if (!image_data) {
		PyErr_SetString (PyExc_RuntimeError, "Could not call image.getdata");
		return nullptr;
	}

	txp texture;

	if (strcmp (format, "RGB") == 0 || strcmp (format, "RGBA") == 0) {
		txp_mipmap mipmap;
		mipmap.width  = PyLong_AsLong (width);
		mipmap.height = PyLong_AsLong (height);

		PyObject *iter = PyObject_GetIter (image_data);
		if (!iter) return nullptr;

		u64 i           = 0;
		PyObject *bytes = PyUnicode_AsUTF8String (mode);
		if (strcmp (PyBytes_AsString (bytes), "RGB") == 0) {
			mipmap.format = TXP_RGB8;
			mipmap.size   = mipmap.get_size ();
			mipmap.data.resize (mipmap.size);

			while (PyObject *item = PyIter_Next (iter)) {
				u8 r = PyLong_AsLong (PyTuple_GetItem (item, 0));
				u8 g = PyLong_AsLong (PyTuple_GetItem (item, 1));
				u8 b = PyLong_AsLong (PyTuple_GetItem (item, 2));

				mipmap.data[i * 3 + 0] = r;
				mipmap.data[i * 3 + 1] = g;
				mipmap.data[i * 3 + 2] = b;

				i++;
				Py_DECREF (item);
			}
		} else if (strcmp (PyBytes_AsString (bytes), "RGBA") == 0) {
			mipmap.format = TXP_RGBA8;
			mipmap.size   = mipmap.get_size ();
			mipmap.data.resize (mipmap.size);

			while (PyObject *item = PyIter_Next (iter)) {
				u8 r = PyLong_AsLong (PyTuple_GetItem (item, 0));
				u8 g = PyLong_AsLong (PyTuple_GetItem (item, 1));
				u8 b = PyLong_AsLong (PyTuple_GetItem (item, 2));
				u8 a = PyLong_AsLong (PyTuple_GetItem (item, 3));

				mipmap.data[i * 4 + 0] = r;
				mipmap.data[i * 4 + 1] = g;
				mipmap.data[i * 4 + 2] = b;
				mipmap.data[i * 4 + 3] = a;

				i++;
				Py_DECREF (item);
			}
		} else {
			PyErr_SetString (PyExc_RuntimeError, "Image mode must be RGB or RGBA");
			return nullptr;
		}

		texture.has_cube_map  = false;
		texture.array_size    = 1;
		texture.mipmaps_count = 1;
		texture.mipmaps.push_back (mipmap);

		Py_DECREF (bytes);
		Py_DECREF (iter);
	} else if (strcmp (format, "BC3") == 0 || strcmp (format, "DXT5") == 0) {
		PyObject *iter = PyObject_GetIter (image_data);
		if (!iter) return nullptr;
		u8 *data = (u8 *)malloc (PyLong_AsLong (width) * PyLong_AsLong (height) * 4);
		u64 i    = 0;

		PyObject *bytes = PyUnicode_AsUTF8String (mode);
		if (strcmp (PyBytes_AsString (bytes), "RGB") == 0) {
			while (PyObject *item = PyIter_Next (iter)) {
				u8 r = PyLong_AsLong (PyTuple_GetItem (item, 0));
				u8 g = PyLong_AsLong (PyTuple_GetItem (item, 1));
				u8 b = PyLong_AsLong (PyTuple_GetItem (item, 2));

				data[i * 4 + 0] = r;
				data[i * 4 + 1] = g;
				data[i * 4 + 2] = b;
				data[i * 4 + 3] = 255;

				i++;
				Py_DECREF (item);
			}
		} else if (strcmp (PyBytes_AsString (bytes), "RGBA") == 0) {
			while (PyObject *item = PyIter_Next (iter)) {
				u8 r = PyLong_AsLong (PyTuple_GetItem (item, 0));
				u8 g = PyLong_AsLong (PyTuple_GetItem (item, 1));
				u8 b = PyLong_AsLong (PyTuple_GetItem (item, 2));
				u8 a = PyLong_AsLong (PyTuple_GetItem (item, 3));

				data[i * 4 + 0] = r;
				data[i * 4 + 1] = g;
				data[i * 4 + 2] = b;
				data[i * 4 + 3] = a;

				i++;
				Py_DECREF (item);
			}
		} else {
			PyErr_SetString (PyExc_RuntimeError, "Image mode must be RGB or RGBA");
			return nullptr;
		}

		Py_DECREF (bytes);
		Py_DECREF (iter);

		txp_mipmap mipmap;

		mipmap.width  = PyLong_AsLong (width);
		mipmap.height = PyLong_AsLong (height);
		mipmap.format = TXP_BC3;
		mipmap.size   = mipmap.get_size ();
		mipmap.data.resize (mipmap.size);

		u8 *dest = mipmap.data.data ();
		for (i32 i = 0; i < mipmap.height; i += 4) {
			i32 remainHeight = std::min<i32> (4, mipmap.height - i);
			for (i32 j = 0; j < mipmap.width; j += 4) {
				i32 remainWidth = std::min<i32> (4, mipmap.width - i);

				HDRColorA color[16] = {};
				for (i32 h = 0; h < remainHeight; h++) {
					for (i32 w = 0; w < remainWidth; w++) {
						color[h * 4 + w].r = (f32)(*(u8 *)(data + ((i + h) * mipmap.width + j + w) * 4 + 0)) / 255.0;
						color[h * 4 + w].g = (f32)(*(u8 *)(data + ((i + h) * mipmap.width + j + w) * 4 + 1)) / 255.0;
						color[h * 4 + w].b = (f32)(*(u8 *)(data + ((i + h) * mipmap.width + j + w) * 4 + 2)) / 255.0;
						color[h * 4 + w].a = (f32)(*(u8 *)(data + ((i + h) * mipmap.width + j + w) * 4 + 3)) / 255.0;
					}
				}

				D3DXEncodeBC3 (dest, color, BC_FLAGS_DITHER_RGB | BC_FLAGS_DITHER_A);
				dest += 16;
			}
		}

		texture.has_cube_map  = false;
		texture.array_size    = 1;
		texture.mipmaps_count = 1;
		texture.mipmaps.push_back (mipmap);
	} else if (strcmp (format, "BC5") == 0 || strcmp (format, "ATI2") == 0) {
		PyObject *iter = PyObject_GetIter (image_data);
		if (!iter) return nullptr;

		const __m128 matrix_y  = {0.212593317f, 0.715214610f, 0.0721921176f, 1.0f};
		const __m128 matrix_cb = {-0.114568502f, -0.385435730f, 0.5000042320f, 1.0f};
		const __m128 matrix_cr = {0.500004232f, -0.454162151f, -0.0458420813f, 1.0f};

		u8 *ya_data   = (u8 *)malloc (PyLong_AsLong (width) * PyLong_AsLong (height) * 2);
		u8 *cbcr_data = (u8 *)malloc (PyLong_AsLong (width) * PyLong_AsLong (height) * 2);

		PyObject *bytes = PyUnicode_AsUTF8String (mode);
		if (strcmp (PyBytes_AsString (bytes), "RGBA") == 0) {
			u64 i = 0;
			while (PyObject *item = PyIter_Next (iter)) {
				f32 r = PyLong_AsLong (PyTuple_GetItem (item, 0));
				f32 g = PyLong_AsLong (PyTuple_GetItem (item, 1));
				f32 b = PyLong_AsLong (PyTuple_GetItem (item, 2));
				u8 a  = PyLong_AsLong (PyTuple_GetItem (item, 3));

#ifdef __x86_64__
				__m128 rgb  = {r, g, b, 128.5};
				__m128 y    = _mm_mul_ps (rgb, matrix_y);
				__m128 cb   = _mm_mul_ps (rgb, matrix_cb);
				__m128 cr   = _mm_mul_ps (rgb, matrix_cr);
				__m128 cbcr = _mm_hadd_ps (cb, cr);

				ya_data[i * 2 + 0]   = y[0] + y[1] + y[2];
				ya_data[i * 2 + 1]   = a;
				cbcr_data[i * 2 + 0] = (cbcr[0] + cbcr[1]) / 1.003922f;
				cbcr_data[i * 2 + 1] = (cbcr[2] + cbcr[3]) / 1.003922f;
#else
				f32 y  = r * 0.212593317f + g * 0.715214610f + b * 0.0721921176f;
				f32 cb = (r * -0.114568502f + g * -0.385435730f + b * 0.5000042320f + 128.5f) / 1.003922f;
				f32 cr = (r * 0.500004232f + g * -0.454162151f + b * -0.0458420813f + 128.5f) / 1.003922f;

				ya_data[i * 2 + 0]   = y[0] + y[1] + y[2];
				ya_data[i * 2 + 1]   = a;
				cbcr_data[i * 2 + 0] = (cbcr[0] + cbcr[1]) / 1.003922f;
				cbcr_data[i * 2 + 1] = (cbcr[2] + cbcr[3]) / 1.003922f;
#endif

				i++;
				Py_DECREF (item);
			}
		} else if (strcmp (PyBytes_AsString (bytes), "RGB") == 0) {
			u64 i = 0;
			while (PyObject *item = PyIter_Next (iter)) {
				f32 r = PyLong_AsLong (PyTuple_GetItem (item, 0));
				f32 g = PyLong_AsLong (PyTuple_GetItem (item, 1));
				f32 b = PyLong_AsLong (PyTuple_GetItem (item, 2));

#ifdef __x86_64__
				__m128 rgb  = {r, g, b, 128.5};
				__m128 y    = _mm_mul_ps (rgb, matrix_y);
				__m128 cb   = _mm_mul_ps (rgb, matrix_cb);
				__m128 cr   = _mm_mul_ps (rgb, matrix_cr);
				__m128 cbcr = _mm_hadd_ps (cb, cr);

				ya_data[i * 2 + 0]   = y[0] + y[1] + y[2];
				ya_data[i * 2 + 1]   = 255;
				cbcr_data[i * 2 + 0] = (cbcr[0] + cbcr[1]) / 1.003922f;
				cbcr_data[i * 2 + 1] = (cbcr[2] + cbcr[3]) / 1.003922f;
#else
				f32 y  = r * 0.212593317f + g * 0.715214610f + b * 0.0721921176f;
				f32 cb = (r * -0.114568502f + g * -0.385435730f + b * 0.5000042320f + 128.5f) / 1.003922f;
				f32 cr = (r * 0.500004232f + g * -0.454162151f + b * -0.0458420813f + 128.5f) / 1.003922f;

				ya_data[i * 2 + 0]   = y[0] + y[1] + y[2];
				ya_data[i * 2 + 1]   = 255;
				cbcr_data[i * 2 + 0] = (cbcr[0] + cbcr[1]) / 1.003922f;
				cbcr_data[i * 2 + 1] = (cbcr[2] + cbcr[3]) / 1.003922f;
#endif

				i++;
				Py_DECREF (item);
			}
		} else {
			PyErr_SetString (PyExc_RuntimeError, "Image mode must be RGB or RGBA");
			return nullptr;
		}
		Py_DECREF (bytes);
		Py_DECREF (iter);

		txp_mipmap ya_mipmap;
		txp_mipmap cbcr_mipmap;

		ya_mipmap.width  = PyLong_AsLong (width);
		ya_mipmap.height = PyLong_AsLong (height);
		ya_mipmap.format = TXP_BC5;
		ya_mipmap.size   = ya_mipmap.get_size ();
		ya_mipmap.data.resize (ya_mipmap.size);

		u8 *dest = ya_mipmap.data.data ();
		for (i32 i = 0; i < ya_mipmap.height; i += 4) {
			i32 remainHeight = std::min<i32> (4, ya_mipmap.height - i);
			for (i32 j = 0; j < ya_mipmap.width; j += 4) {
				i32 remainWidth = std::min<i32> (4, ya_mipmap.width - i);

				XMFLOAT2 color[16] = {0.0};
				for (i32 h = 0; h < remainHeight; h++) {
					for (i32 w = 0; w < remainWidth; w++) {
						color[h * 4 + w].x = (f32)(*(u8 *)(ya_data + ((i + h) * ya_mipmap.width + j + w) * 2 + 0)) / 255.0;
						color[h * 4 + w].y = (f32)(*(u8 *)(ya_data + ((i + h) * ya_mipmap.width + j + w) * 2 + 1)) / 255.0;
					}
				}

				D3DXEncodeBC5U (dest, color);
				dest += 16;
			}
		}

		cbcr_mipmap.width  = PyLong_AsLong (width) / 2;
		cbcr_mipmap.height = PyLong_AsLong (height) / 2;
		cbcr_mipmap.format = TXP_BC5;
		cbcr_mipmap.size   = cbcr_mipmap.get_size ();
		cbcr_mipmap.data.resize (cbcr_mipmap.size);

		dest = cbcr_mipmap.data.data ();
		for (i32 i = 0; i < ya_mipmap.height; i += 8) {
			i32 remainHeight = std::min<i32> (8, ya_mipmap.height - i);
			for (i32 j = 0; j < ya_mipmap.width; j += 8) {
				i32 remainWidth = std::min<i32> (8, ya_mipmap.width - i);

				f32 color[8][8][2] = {0.5};
				for (i32 h = 0; h < remainHeight; h++) {
					for (i32 w = 0; w < remainWidth; w++) {
						color[h][w][0] = (f32)(*(u8 *)(cbcr_data + ((i + h) * ya_mipmap.width + j + w) * 2 + 0)) / 255.0;
						color[h][w][1] = (f32)(*(u8 *)(cbcr_data + ((i + h) * ya_mipmap.width + j + w) * 2 + 1)) / 255.0;
					}
				}

				XMFLOAT2 temp[16];
				for (i32 h = 0; h < 4; h++) {
					for (i32 w = 0; w < 4; w++) {
						temp[h * 4 + w].x =
						    (color[h * 2][w * 2][0] + color[h * 2][w * 2 + 1][0] + color[h * 2 + 1][w * 2][0] + color[h * 2 + 1][w * 2 + 1][0]) / 4.0;
						temp[h * 4 + w].y =
						    (color[h * 2][w * 2][1] + color[h * 2][w * 2 + 1][1] + color[h * 2 + 1][w * 2][1] + color[h * 2 + 1][w * 2 + 1][1]) / 4.0;
					}
				}
				D3DXEncodeBC5U (dest, temp);
				dest += 16;
			}
		}

		texture.has_cube_map  = false;
		texture.array_size    = 1;
		texture.mipmaps_count = 2;
		texture.mipmaps.push_back (ya_mipmap);
		texture.mipmaps.push_back (cbcr_mipmap);
	} else if (strcmp (format, "BC7") == 0) {
		PyObject *iter = PyObject_GetIter (image_data);
		if (!iter) return nullptr;
		u8 *data = (u8 *)malloc (PyLong_AsLong (width) * PyLong_AsLong (height) * 4);
		u64 i    = 0;

		PyObject *bytes = PyUnicode_AsUTF8String (mode);
		if (strcmp (PyBytes_AsString (bytes), "RGB") == 0) {
			while (PyObject *item = PyIter_Next (iter)) {
				u8 r = PyLong_AsLong (PyTuple_GetItem (item, 0));
				u8 g = PyLong_AsLong (PyTuple_GetItem (item, 1));
				u8 b = PyLong_AsLong (PyTuple_GetItem (item, 2));

				data[i * 4 + 0] = r;
				data[i * 4 + 1] = g;
				data[i * 4 + 2] = b;
				data[i * 4 + 3] = 255;

				i++;
				Py_DECREF (item);
			}
		} else if (strcmp (PyBytes_AsString (bytes), "RGBA") == 0) {
			while (PyObject *item = PyIter_Next (iter)) {
				u8 r = PyLong_AsLong (PyTuple_GetItem (item, 0));
				u8 g = PyLong_AsLong (PyTuple_GetItem (item, 1));
				u8 b = PyLong_AsLong (PyTuple_GetItem (item, 2));
				u8 a = PyLong_AsLong (PyTuple_GetItem (item, 3));

				data[i * 4 + 0] = r;
				data[i * 4 + 1] = g;
				data[i * 4 + 2] = b;
				data[i * 4 + 3] = a;

				i++;
				Py_DECREF (item);
			}
		} else {
			PyErr_SetString (PyExc_RuntimeError, "Image mode must be RGB or RGBA");
			return nullptr;
		}

		Py_DECREF (bytes);
		Py_DECREF (iter);

		txp_mipmap mipmap;

		mipmap.width  = PyLong_AsLong (width);
		mipmap.height = PyLong_AsLong (height);
		mipmap.format = (txp_format)15; // BC7
		mipmap.size   = mipmap.width * mipmap.height;
		mipmap.data.resize (mipmap.size);

		u8 *dest = mipmap.data.data ();
		std::vector<std::thread> thread_pool;
		std::vector<HDRColorA *> ptrs;

		for (i32 i = 0; i < mipmap.height; i += 4) {
			i32 remainHeight = std::min<i32> (4, mipmap.height - i);
			for (i32 j = 0; j < mipmap.width; j += 4) {
				i32 remainWidth = std::min<i32> (4, mipmap.width - i);

				HDRColorA *color = (HDRColorA *)calloc (16, sizeof (HDRColorA));
				for (i32 h = 0; h < remainHeight; h++) {
					for (i32 w = 0; w < remainWidth; w++) {
						color[h * 4 + w].r = (f32)(*(u8 *)(data + ((i + h) * mipmap.width + j + w) * 4 + 0)) / 255.0;
						color[h * 4 + w].g = (f32)(*(u8 *)(data + ((i + h) * mipmap.width + j + w) * 4 + 1)) / 255.0;
						color[h * 4 + w].b = (f32)(*(u8 *)(data + ((i + h) * mipmap.width + j + w) * 4 + 2)) / 255.0;
						color[h * 4 + w].a = (f32)(*(u8 *)(data + ((i + h) * mipmap.width + j + w) * 4 + 3)) / 255.0;
					}
				}

				thread_pool.push_back (std::thread (D3DXEncodeBC7, dest, color, 0));
				ptrs.push_back (color);
				dest += 16;
			}
		}

		for (auto &t : thread_pool)
			t.join ();

		for (auto ptr : ptrs)
			free (ptr);

		texture.has_cube_map  = false;
		texture.array_size    = 1;
		texture.mipmaps_count = 1;
		texture.mipmaps.push_back (mipmap);
	} else {
		PyErr_SetString (PyExc_RuntimeError, "Unknown pixel format");
		return nullptr;
	}

	self->real->textures.push_back (texture);
	self->names->push_back (std::string (name));

	Py_DECREF (width);
	Py_DECREF (height);
	Py_DECREF (mode);

	Py_RETURN_NONE;
}

static PyObject *
py_txp_set_get_texture_id (pyobject_txp_set *self, PyObject *args) {
	const char *name;
	if (!PyArg_ParseTuple (args, "s", &name)) return nullptr;

	for (u64 i = 0; i < self->names->size (); i++)
		if (strcmp (name, self->names->at (i).c_str ()) == 0) return PyLong_FromLong (i);

	PyErr_SetString (PyExc_RuntimeError, "Could not find texture");
	return nullptr;
}

static PyMethodDef pymethods_txp_set[] = {{"add_texture_data", (PyCFunction)py_txp_set_add_texture_data, METH_VARARGS,
                                           "Add textures to set (name, width, height, format: [RGB, RGBA, BC1/DXT1, BC2/DXT3, BC3/DXT5], data)"},
                                          {"add_texture_pillow", (PyCFunction)py_txp_set_add_texture_pillow, METH_VARARGS,
                                           "Add a texture from pillow (name, image, compression: [RGB/RGBA, BC5/ATI2]])"},
                                          {"get_texture_id", (PyCFunction)py_txp_set_get_texture_id, METH_VARARGS, "Get the id for a texture (name)"},
                                          {nullptr}};

static PyType_Slot pyslots_txp_set[] = {
    {Py_tp_init, (void *)py_txp_set_init},
    {Py_tp_finalize, (void *)py_txp_set_finalize},
    {Py_tp_methods, pymethods_txp_set},
    {0},
};

PYTHON_TYPE_DEF (txp_set);

struct pyobject_sprite_info {
	PyObject_HEAD;
	spr::SprInfo spr_info;
	SpriteData spr_data;
	std::string *name;
};

static int
py_sprite_info_init (pyobject_sprite_info *self, PyObject *args, PyObject *kwds) {
	self->name = new std::string ();
	return 0;
}

void
py_sprite_info_finalize (pyobject_sprite_info *self) {
	delete self->name;
}

static PyObject *
py_sprite_info_name_get (pyobject_sprite_info *self, void *closure) {
	return PyUnicode_FromString (self->name->c_str ());
}

static int
py_sprite_info_name_set (pyobject_sprite_info *self, PyObject *value, void *closure) {
	if (value == nullptr || !PyUnicode_Check (value)) {
		PyErr_SetString (PyExc_TypeError, "Name must be string");
		return -1;
	}

	PyObject *bytes = PyUnicode_AsUTF8String (value);
	self->name->assign (PyBytes_AsString (bytes));
	Py_DECREF (bytes);

	return 0;
}

static PyObject *
py_sprite_info_resolution_mode_get (pyobject_sprite_info *self, void *closure) {
	switch ((i32)self->spr_data.resolution_mode) {
	case 0x00: return PyUnicode_FromString ("QVGA");
	case 0x01: return PyUnicode_FromString ("VGA");
	case 0x02: return PyUnicode_FromString ("SVGA");
	case 0x03: return PyUnicode_FromString ("XGA");
	case 0x04: return PyUnicode_FromString ("SXGA");
	case 0x05: return PyUnicode_FromString ("SXGAPlus");
	case 0x06: return PyUnicode_FromString ("UXGA");
	case 0x07: return PyUnicode_FromString ("WVGA");
	case 0x08: return PyUnicode_FromString ("WSVGA");
	case 0x09: return PyUnicode_FromString ("WXGA");
	case 0x0A: return PyUnicode_FromString ("FWXGA");
	case 0x0B: return PyUnicode_FromString ("WUXGA");
	case 0x0C: return PyUnicode_FromString ("WQXGA");
	case 0x0D: return PyUnicode_FromString ("HD");
	case 0x0E: return PyUnicode_FromString ("FHD");
	case 0x0F: return PyUnicode_FromString ("UHD");
	case 0x10: return PyUnicode_FromString ("3KatUHD");
	case 0x11: return PyUnicode_FromString ("3K");
	case 0x12: return PyUnicode_FromString ("QHD");
	case 0x13: return PyUnicode_FromString ("WQVGA");
	case 0x14: return PyUnicode_FromString ("qHD");
	case 0x15: return PyUnicode_FromString ("XGAPlus");
	case 0x16: return PyUnicode_FromString ("1176x664");
	case 0x17: return PyUnicode_FromString ("1200x960");
	case 0x18: return PyUnicode_FromString ("WXGA1280x900");
	case 0x19: return PyUnicode_FromString ("SXGAMinus");
	case 0x1A: return PyUnicode_FromString ("FWXGA1366x768");
	case 0x1B: return PyUnicode_FromString ("WXGAPlus");
	case 0x1C: return PyUnicode_FromString ("HDPlus");
	case 0x1D: return PyUnicode_FromString ("WSXGA");
	case 0x1E: return PyUnicode_FromString ("WSXGAPlus");
	case 0x1F: return PyUnicode_FromString ("1920x1440");
	case 0x20: return PyUnicode_FromString ("QWXGA");
	default: {
		PyErr_SetString (PyExc_TypeError, "Unknnown Resolution mode");
		return nullptr;
	}; break;
	}
}

static int
py_sprite_info_resolution_mode_set (pyobject_sprite_info *self, PyObject *value, void *closure) {
	if (value == nullptr || !PyUnicode_Check (value)) {
		PyErr_SetString (PyExc_TypeError, "Resolution mode must be string");
		return -1;
	}

	PyObject *bytes    = PyUnicode_AsUTF8String (value);
	const char *string = PyBytes_AsString (bytes);
	if (strcmp (string, "QVGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x00;
	else if (strcmp (string, "VGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x01;
	else if (strcmp (string, "SVGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x02;
	else if (strcmp (string, "XGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x03;
	else if (strcmp (string, "SXGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x04;
	else if (strcmp (string, "SXGAPlus") == 0) self->spr_data.resolution_mode = (resolution_mode)0x05;
	else if (strcmp (string, "UXGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x06;
	else if (strcmp (string, "WVGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x07;
	else if (strcmp (string, "WSVGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x08;
	else if (strcmp (string, "WXGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x09;
	else if (strcmp (string, "FWXGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x0A;
	else if (strcmp (string, "WUXGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x0B;
	else if (strcmp (string, "WQXGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x0C;
	else if (strcmp (string, "HD") == 0) self->spr_data.resolution_mode = (resolution_mode)0x0D;
	else if (strcmp (string, "HDTV720") == 0) self->spr_data.resolution_mode = (resolution_mode)0x0D;
	else if (strcmp (string, "FHD") == 0) self->spr_data.resolution_mode = (resolution_mode)0x0E;
	else if (strcmp (string, "HDTV1080") == 0) self->spr_data.resolution_mode = (resolution_mode)0x0E;
	else if (strcmp (string, "UHD") == 0) self->spr_data.resolution_mode = (resolution_mode)0x0F;
	else if (strcmp (string, "3KatUHD") == 0) self->spr_data.resolution_mode = (resolution_mode)0x10;
	else if (strcmp (string, "3K") == 0) self->spr_data.resolution_mode = (resolution_mode)0x11;
	else if (strcmp (string, "QHD") == 0) self->spr_data.resolution_mode = (resolution_mode)0x12;
	else if (strcmp (string, "WQVGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x13;
	else if (strcmp (string, "qHD") == 0) self->spr_data.resolution_mode = (resolution_mode)0x14;
	else if (strcmp (string, "XGAPlus") == 0) self->spr_data.resolution_mode = (resolution_mode)0x15;
	else if (strcmp (string, "1176x664") == 0) self->spr_data.resolution_mode = (resolution_mode)0x16;
	else if (strcmp (string, "1200x960") == 0) self->spr_data.resolution_mode = (resolution_mode)0x17;
	else if (strcmp (string, "WXGA1280x900") == 0) self->spr_data.resolution_mode = (resolution_mode)0x18;
	else if (strcmp (string, "SXGAMinus") == 0) self->spr_data.resolution_mode = (resolution_mode)0x19;
	else if (strcmp (string, "FXWGA1366x768") == 0) self->spr_data.resolution_mode = (resolution_mode)0x1A;
	else if (strcmp (string, "WXGAPlus") == 0) self->spr_data.resolution_mode = (resolution_mode)0x1B;
	else if (strcmp (string, "HDPlus") == 0) self->spr_data.resolution_mode = (resolution_mode)0x1C;
	else if (strcmp (string, "WSXGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x1D;
	else if (strcmp (string, "WSXGAPLus") == 0) self->spr_data.resolution_mode = (resolution_mode)0x1E;
	else if (strcmp (string, "1920x1440") == 0) self->spr_data.resolution_mode = (resolution_mode)0x1F;
	else if (strcmp (string, "QXWGA") == 0) self->spr_data.resolution_mode = (resolution_mode)0x20;
	else {
		PyErr_SetString (PyExc_TypeError, "Unknown resolution mode");
		return -1;
	}
	Py_DECREF (bytes);

	return 0;
}

static PyGetSetDef pygetsets_sprite_info[] = {
    {"name", (getter)py_sprite_info_name_get, (setter)py_sprite_info_name_set, nullptr, nullptr},
    {"resolution_mode", (getter)py_sprite_info_resolution_mode_get, (setter)py_sprite_info_resolution_mode_set, nullptr, nullptr},
    {nullptr}};

static PyMemberDef pymembers_sprite_info[] = {
    {"texid", T_UINT, offsetof (pyobject_sprite_info, spr_info) + offsetof (spr::SprInfo, texid), 0, nullptr},
    {"rotate", T_INT, offsetof (pyobject_sprite_info, spr_info) + offsetof (spr::SprInfo, rotate), 0, nullptr},
    {"x", T_FLOAT, offsetof (pyobject_sprite_info, spr_info) + offsetof (spr::SprInfo, px), 0, nullptr},
    {"y", T_FLOAT, offsetof (pyobject_sprite_info, spr_info) + offsetof (spr::SprInfo, py), 0, nullptr},
    {"width", T_FLOAT, offsetof (pyobject_sprite_info, spr_info) + offsetof (spr::SprInfo, width), 0, nullptr},
    {"height", T_FLOAT, offsetof (pyobject_sprite_info, spr_info) + offsetof (spr::SprInfo, height), 0, nullptr},
    {"attr", T_UINT, offsetof (pyobject_sprite_info, spr_data) + offsetof (SpriteData, attr), 0, nullptr},
    {nullptr}};

static PyType_Slot pyslots_sprite_info[] = {{Py_tp_init, (void *)py_sprite_info_init},
                                            {Py_tp_finalize, (void *)py_sprite_info_finalize},
                                            {Py_tp_getset, pygetsets_sprite_info},
                                            {Py_tp_members, pymembers_sprite_info},
                                            {0}};

PYTHON_TYPE_DEF (sprite_info);

struct pyobject_spr_set {
	PyObject_HEAD;
	spr_set real;
};

static int
py_spr_set_init (pyobject_spr_set *self, PyObject *args, PyObject *kwds) {
	self->real.ready      = true;
	self->real.modern     = false;
	self->real.big_endian = false;
	self->real.is_x       = false;

	self->real.flag           = 0;
	self->real.num_of_texture = 0;
	self->real.num_of_sprite  = 0;
	self->real.sprinfo        = nullptr;
	self->real.texname        = nullptr;
	self->real.sprname        = nullptr;
	self->real.sprdata        = nullptr;
	self->real.txp            = nullptr;

	return 0;
}

void
py_spr_set_finalize (pyobject_spr_set *self) {
	if (self->real.sprinfo != nullptr) free (self->real.sprinfo);
	if (self->real.texname != nullptr) {
		for (i32 i = 0; i < self->real.num_of_texture; i++)
			free ((void *)self->real.texname[i]);
		free (self->real.texname);
	}
	if (self->real.sprname != nullptr) {
		for (i32 i = 0; i < self->real.num_of_sprite; i++)
			free ((void *)self->real.sprname[i]);
		free (self->real.sprname);
	}
	if (self->real.sprdata != nullptr) free (self->real.sprdata);
	if (self->real.txp != nullptr) delete self->real.txp;
}

static int
py_spr_set_txp_set (pyobject_spr_set *self, PyObject *value, void *closure) {
	if (!PyObject_TypeCheck (value, pytype_txp_set)) {
		PyErr_SetString (PyExc_TypeError, "txp must be KKdLib.txp_set");
		return -1;
	}

	pyobject_txp_set *txp = (pyobject_txp_set *)value;
	if (txp->real->textures.size () == 0) {
		PyErr_SetString (PyExc_TypeError, "txp must have textures");
		return -1;
	}

	if (self->real.texname != nullptr) {
		for (i32 i = 0; i < self->real.num_of_texture; i++)
			free ((void *)self->real.texname[i]);
		free (self->real.texname);
	}
	if (self->real.txp != nullptr) free (self->real.txp);

	self->real.num_of_texture = txp->real->textures.size ();
	self->real.texname        = (const char **)malloc (sizeof (char *) * txp->names->size ());
	for (u64 i = 0; i < txp->names->size (); i++) {
		self->real.texname[i] = (const char *)calloc (txp->names->at (i).size () + 1, sizeof (char));
		strcpy ((char *)self->real.texname[i], txp->names->at (i).c_str ());
	}

	self->real.txp  = new txp_set ();
	*self->real.txp = *txp->real;

	return 0;
}

static PyObject *
py_spr_set_add_sprite (pyobject_spr_set *self, PyObject *args) {
	pyobject_sprite_info *sprite_info;
	if (!PyArg_ParseTuple (args, "O!", pytype_sprite_info, &sprite_info)) return nullptr;

	if (self->real.num_of_sprite == 0) {
		self->real.sprinfo    = (spr::SprInfo *)malloc (sizeof (spr::SprInfo));
		self->real.sprinfo[0] = sprite_info->spr_info;

		self->real.sprname    = (const char **)malloc (sizeof (char *));
		self->real.sprname[0] = (const char *)calloc (sprite_info->name->size () + 1, sizeof (char));
		strcpy ((char *)self->real.sprname[0], sprite_info->name->c_str ());

		self->real.sprdata    = (SpriteData *)malloc (sizeof (SpriteData));
		self->real.sprdata[0] = sprite_info->spr_data;
	} else {
		spr::SprInfo *new_sprinfo = (spr::SprInfo *)malloc (sizeof (spr::SprInfo) * (self->real.num_of_sprite + 1));
		memcpy (new_sprinfo, self->real.sprinfo, sizeof (spr::SprInfo) * self->real.num_of_sprite);
		new_sprinfo[self->real.num_of_sprite] = sprite_info->spr_info;
		free (self->real.sprinfo);
		self->real.sprinfo = new_sprinfo;

		const char **new_sprname = (const char **)malloc (sizeof (char *) * (self->real.num_of_sprite + 1));
		memcpy (new_sprname, self->real.sprname, sizeof (char *) * self->real.num_of_sprite);
		new_sprname[self->real.num_of_sprite] = (const char *)calloc (sprite_info->name->size () + 1, sizeof (char));
		strcpy ((char *)new_sprname[self->real.num_of_sprite], sprite_info->name->c_str ());
		free (self->real.sprname);
		self->real.sprname = new_sprname;

		SpriteData *new_sprdata = (SpriteData *)malloc (sizeof (SpriteData) * (self->real.num_of_sprite + 1));
		memcpy (new_sprdata, self->real.sprdata, sizeof (SpriteData) * self->real.num_of_sprite);
		new_sprdata[self->real.num_of_sprite] = sprite_info->spr_data;
		free (self->real.sprdata);
		self->real.sprdata = new_sprdata;
	}

	self->real.num_of_sprite++;

	Py_RETURN_NONE;
}

static PyObject *
py_spr_set_pack (pyobject_spr_set *self, PyObject *args) {
	if (self->real.num_of_texture == 0 || self->real.num_of_sprite == 0) {
		PyErr_SetString (PyExc_TypeError, "Must set txp and sprites");
		return nullptr;
	}

	for (i32 i = 0; i < self->real.num_of_sprite; i++) {
		txp texture              = self->real.txp->textures.at (self->real.sprinfo[i].texid);
		self->real.sprinfo[i].su = self->real.sprinfo[i].px / texture.mipmaps[0].width;
		self->real.sprinfo[i].sv = self->real.sprinfo[i].py / texture.mipmaps[0].height;
		self->real.sprinfo[i].eu = (self->real.sprinfo[i].px + self->real.sprinfo[i].width) / texture.mipmaps[0].width;
		self->real.sprinfo[i].ev = (self->real.sprinfo[i].py + self->real.sprinfo[i].height) / texture.mipmaps[0].height;
	}

	void *data = nullptr;
	size_t size;
	self->real.pack_file (&data, &size);

	return PyBytes_FromStringAndSize ((const char *)data, size);
}

static PyGetSetDef pygetsets_spr_set[] = {
    {"txp", nullptr, (setter)py_spr_set_txp_set, nullptr, nullptr},
    {nullptr},
};

static PyMethodDef pymethods_spr_set[] = {{"add_sprite", (PyCFunction)py_spr_set_add_sprite, METH_VARARGS, "Add sprite to set (spite_info)"},
                                          {"pack", (PyCFunction)py_spr_set_pack, METH_VARARGS, "Pack sprite set to bytes ()"},
                                          {0}};

static PyType_Slot pyslots_spr_set[] = {{Py_tp_init, (void *)py_spr_set_init},
                                        {Py_tp_finalize, (void *)py_spr_set_finalize},
                                        {Py_tp_getset, pygetsets_spr_set},
                                        {Py_tp_methods, pymethods_spr_set},
                                        {0}};

PYTHON_TYPE_DEF (spr_set);

static int
KKdLib_module_exec (PyObject *m) {
	PYTHON_TYPE_INIT (farc);
	PYTHON_TYPE_INIT (farc_file);

	PYTHON_TYPE_INIT (txp_set);
	PYTHON_TYPE_INIT (sprite_info);
	PYTHON_TYPE_INIT (spr_set);

	return 0;
}

static PyModuleDef_Slot KKdLib_module_slots[] = {{Py_mod_exec, (void *)KKdLib_module_exec}, {0, nullptr}};

static PyModuleDef KKdLib_module = {
    .m_base  = PyModuleDef_HEAD_INIT,
    .m_name  = "KorenKonder diva Library",
    .m_doc   = PyDoc_STR ("KKdLib python wrapper"),
    .m_size  = 0,
    .m_slots = KKdLib_module_slots,
};

// KKdLib AES requirement
bool cpu_caps_aes_ni = false;

PyMODINIT_FUNC
PyInit_KKdLib () {
#ifdef __x86_64__
	u32 cpuid_eax = 1;
	u32 cpuid_ebx = 0;
	u32 cpuid_ecx = 0;
	u32 cpuid_edx = 0;
	__get_cpuid (1, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);
	cpu_caps_aes_ni = !!(cpuid_ecx & (1 << 25));
#endif

	return PyModuleDef_Init (&KKdLib_module);
}
