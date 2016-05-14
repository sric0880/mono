/*
 * image.c: Routines for manipulating an image stored in an
 * extended PE/COFF file.
 * 
 * Authors:
 *   Miguel de Icaza (miguel@ximian.com)
 *   Paolo Molaro (lupus@ximian.com)
 *
 * Copyright 2001-2003 Ximian, Inc (http://www.ximian.com)
 * Copyright 2004-2009 Novell, Inc (http://www.novell.com)
 *
 */
#include "image.h"

#define INVALID_ADDRESS 0xffffffff

// code by kun 2015.08.03
// implementation of the RC6 encryption algorithm
void mono_image_open_from_data_with_name_(char** lpdata, guint32* lpdata_len, const char* name/*, MonoImageOpenStatus *status*/, gboolean* copyed);
void try_capture_unity_engine_dll(const char *name, MonoImage *image);
// void try_modify_version_info(const char *name, MonoImage *image);