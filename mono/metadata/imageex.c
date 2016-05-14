//------------------------
//kun 2016.05.01
//------------------------
#include <config.h>
#include <stdio.h>
#include <glib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include "imageex.h"
#include "cil-coff.h"
#include "mono-endian.h"
#include "tabledefs.h"
#include "tokentype.h"
#include "metadata-internals.h"
#include "profiler-private.h"
#include "loader.h"
#include "marshal.h"
#include "coree.h"
#include <mono/io-layer/io-layer.h>
#include <mono/utils/mono-logger.h>
#include <mono/utils/mono-path.h>
#include <mono/utils/mono-mmap.h>
#include <mono/utils/mono-io-portability.h>
#include <mono/metadata/class-internals.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/object-internals.h>
#include <mono/metadata/security-core-clr.h>
#include <mono/metadata/verify-internals.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

//------------------------------------------------------------
// code by kun 2015.08.03
// hot update dll strategy
//------------------------------------------------------------
gboolean _check_dll_name(const char *name, const char *dll_name)
{
    if (!name || !dll_name)
    {
        return FALSE;
    }
    char* sname = g_path_get_basename(name);
    gboolean ret = strcmp(sname, dll_name) == 0 ? TRUE:FALSE;
    g_free(sname);
    return ret;
}

// gboolean _check_ff_dll(const char *name)
// {
//     return _check_dll_name(name, "Assembly-CSharp.dll")
//     || _check_dll_name(name, "Assembly-CSharp-firstpass.dll");
// }

gboolean _check_ff_main_dll(const char* name)
{
    return _check_dll_name(name, "Assembly-CSharp.dll");
}

gboolean _check_unity_engine_dll(const char* name)
{
    return _check_dll_name(name, "UnityEngine.dll");
}

MonoImage *unity_engine_image = NULL;
MonoImage *temp_unity_engine_image = NULL;
MonoDomain *temp_domain = NULL;

void try_capture_unity_engine_dll(const char *name, MonoImage *image)
{
    if( _check_unity_engine_dll(name) )
    {
        unity_engine_image = image;
    }

    if( _check_ff_main_dll(name) )
    {
        if(unity_engine_image == NULL && temp_unity_engine_image == NULL)
        {
            temp_domain = mono_jit_init("UnityEngine.dll");
            MonoAssembly *assembly = mono_domain_assembly_open(temp_domain, "UnityEngine.dll");
            temp_unity_engine_image = mono_assembly_get_image(assembly);
        }
        if(unity_engine_image == NULL)
        {
            unity_engine_image = temp_unity_engine_image;
        }
        else if ( unity_engine_image != temp_unity_engine_image && temp_domain)
        {
            mono_jit_cleanup(temp_domain);
            temp_domain = NULL;
            temp_unity_engine_image = NULL;
        }
    }
}

char* _strdup_resource_path()
{
    //MonoImage *unity_engine_image;
    MonoClass *klass = NULL;
    MonoProperty *property = NULL;
    MonoMethod *get_method = NULL;
    MonoObject *property_value = NULL;
    MonoString *cs_string = NULL;
    char *res_path = NULL;
    const char *klass_name = "Application";
    const char *namespace = "UnityEngine";
    const char *property_name = "persistentDataPath";

    klass = mono_class_from_name(unity_engine_image, namespace, klass_name );
    property = mono_class_get_property_from_name(klass, property_name);
    get_method = property->get;
    property_value = (MonoString *)mono_runtime_invoke(get_method, NULL, NULL, NULL);
    if(property_value)
    {
        MonoError error;
        res_path = mono_string_to_utf8_checked (property_value, &error);
        if (!mono_error_ok (&error))
        {
            if (res_path)
            {
                g_free(res_path);
                res_path = NULL;
            }
            mono_error_cleanup (&error);
        }
    }

    if (res_path)
    {
        return res_path;
    }

    return NULL;
}

MonoFileMap* _open_file_map(const char* fname)
{
    MonoFileMap* fmap = NULL;
    fmap = (MonoFileMap *)fopen (fname, "rb");
    if (fmap == NULL)
    {
        if (IS_PORTABILITY_SET)
        {
            gchar *ffname = mono_portability_find_file (fname, TRUE);
            if (ffname)
            {
                fmap = (MonoFileMap *)fopen (ffname, "rb");
                g_free (ffname);
            }
        }
    }

    return fmap;
}

int _close_file_map(MonoFileMap* fmap)
{
    return fclose ((FILE*)fmap);
}

guint64 _file_map_size (MonoFileMap *fmap)
{
    struct stat stat_buf;
    if (fstat (fileno ((FILE*)fmap), &stat_buf) < 0)
        return 0;
    return stat_buf.st_size;
}

int _file_map_fd (MonoFileMap *fmap)
{
    return fileno ((FILE*)fmap);
}

void* _mono_file_map (size_t length, int flags, int fd, guint64 offset, void **ret_handle)
{
    guint64 cur_offset;
    size_t bytes_read;
    void *ptr = malloc (length);
    if (!ptr)
    {
        g_message("ptr is null");
        return NULL;
    }
    cur_offset = lseek (fd, 0, SEEK_CUR);
    if (lseek (fd, offset, SEEK_SET) != offset) {
        g_message("offset error");
        free (ptr);
        return NULL;
    }
    bytes_read = read (fd, ptr, length);
    lseek (fd, cur_offset, SEEK_SET);
    *ret_handle = NULL;
    //g_message("_mono_file_map_done. length=%d", length);
    return ptr;
}

int _mono_file_unmap (void *addr, void *handle)
{
    free(addr);
    return 0;
}

// code by kun 2016.04.26
// dynamic upate dll strategy.
static const char* apk_dll_version = "__kun__dll_version_00000000";
static int res_dll_version = 0;

// MonoImage* mainImage = NULL;
// void try_modify_version_info(const char *name, MonoImage *image)
// {
//     if(_check_ff_main_dll(name))
//     {
//         mainImage = image;
//     }
// }

// void modify_version_info()
// {
//     if ( mainImage )
//     {
//         MonoClass *klass = NULL;
//         MonoProperty *property = NULL;
//         MonoMethod *set_method = NULL;
//         MonoObject *property_value = NULL;
//         void* params[1];
//         char *res_path = NULL;
//         const char *klass_name = "APKVersionInfo";
//         const char *namespace = "Client";
//         const char *property_name = "apk_dll_version";

//         klass = mono_class_from_name(mainImage, namespace, klass_name );
//         if(!klass)
//             return;
//         property = mono_class_get_property_from_name(klass, property_name);
//         if(!property)
//             return;
//         set_method = property->set;
//         if(!set_method)
//             return;
//         //*(int*)mono_object_unbox (result)
        
//         //g_message("prepare set apk version property : %s", apk_dll_version);
//         *params = (MonoObject*)mono_string_new (mono_domain_get (), apk_dll_version);
//         //g_message("params[0] : %p", params[0]);
//         //g_message("prepare set apk version property22222 : %s", apk_dll_version);
//         mono_runtime_invoke(set_method, NULL, params, NULL);
        
//         /*
//         int val = 111;
//         *params = &val;
//         mono_runtime_invoke(set_method, NULL, params, NULL);
//         */
//     }
// }

gboolean check_version_equal()
{
    if( res_dll_version == 0 )
    {
        MonoFileMap* fmap = NULL;
        char *fname = NULL;
        char* res_path = _strdup_resource_path();
        if ( res_path )
        {
            fname = g_strdup_printf("%s/ver.txt", res_path);
            g_free(res_path);
        }

        fmap = _open_file_map(fname);
        if(fname)
        {
            g_free(fname);
        }

        if (fmap == NULL)
        {
            return FALSE;
        }

        if( fmap != NULL )
        {
            int data_len = _file_map_size (fmap);
            void* handle;
            gpointer file_map = _mono_file_map (data_len, MONO_MMAP_READ|MONO_MMAP_PRIVATE, _file_map_fd (fmap), 0, &handle);
            if (file_map != NULL)
            {
                char version_text[16];
                //g_message("data_len: %d", data_len);
                memcpy(version_text, file_map, data_len);
                version_text[data_len] = '\0';
                //g_message("version_text: %s", version_text);
                int version = atoi(version_text);
                res_dll_version = version;
                _mono_file_unmap(file_map, fmap);
            }

            _close_file_map(fmap);
        }
    }

    char *version_str;
    version_str = g_strdup_printf("__kun__dll_version_%08d", res_dll_version);
    g_message("version_str : %s", version_str);
    gboolean ret = strcmp(apk_dll_version, version_str) == 0;
    g_free(version_str);

    return ret;
}


gboolean replace_dll(char* data, guint32 data_len, const char* name, char** replaced_data, guint32* replaced_data_len/*, MonoImageOpenStatus *status*/)
{
    char* fname = NULL;
    MonoFileMap *fmap = NULL;
    guint32 fsize = 0;

	if(check_version_equal())
	{
        char* res_path = _strdup_resource_path();
        if ( res_path )
        {
            char* sname = g_path_get_basename(name);
            fname = g_strdup_printf("%s/dll_apk/%s", res_path, sname);
            g_free(sname);
            g_free(res_path);
        }

        fmap = _open_file_map(fname);

        if(fname)
        {
            g_free(fname);
        }

        if (fmap == NULL)
        {
            // if (status)
            // {
            //     *status = MONO_IMAGE_IMAGE_INVALID;
            // }

            return FALSE;
        }

		fsize = _file_map_size (fmap);

		void* handle;
		gpointer file_map = _mono_file_map (fsize, MONO_MMAP_READ|MONO_MMAP_PRIVATE, _file_map_fd (fmap), 0, &handle);
		if (file_map == NULL)
		{
			// if (status)
			// {
			// 	*status = MONO_IMAGE_ERROR_ERRNO;
			// }
			return FALSE;
		}

		char* datac = g_try_malloc(fsize);
		if ( !datac )
		{
			// if (status)
			// {
			// 	*status = MONO_IMAGE_ERROR_ERRNO;
			// }
			return FALSE;
		}

		memcpy(datac, file_map, fsize);
        _mono_file_unmap(file_map, fmap);
		_close_file_map (fmap);
        *replaced_data = datac;
        *replaced_data_len = fsize;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void mono_image_open_from_data_with_name_(char** lpdata, guint32* lpdata_len, const char* name/*, MonoImageOpenStatus *status*/, gboolean* copyed)
{
    char* data = *lpdata;
    guint32 data_len = *lpdata_len;
    if (data && name)
    {
        // modify_version_info();

        if (_check_ff_main_dll(name))
        {
            char* replaced_data = NULL;
            guint32 replaced_data_len = 0;
#if defined(ANDROID)
            gboolean replaced = replace_dll(data, data_len, name, &replaced_data, &replaced_data_len/*, status */);
            if(replaced)
            {
                *lpdata = replaced_data;
                *lpdata_len = replaced_data_len;
                *copyed = TRUE;
                g_message("replaced %s", name);
            }
#endif
        }
    }
}